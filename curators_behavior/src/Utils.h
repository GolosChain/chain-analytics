#ifndef UTILS_H_
#define UTILS_H_
#include <random>
#include <tuple>
#include <nlopt.hpp>
#include <iostream>
#include <algorithm>
#include <boost/math/distributions.hpp>
#include "pugixml/pugixml.hpp"

namespace Xml
{
	pugi::xml_node getNode(const pugi::xml_node& node, const std::string& relativePath, bool strict = true);
	pugi::xml_attribute getAttribute(const pugi::xml_node& node, const std::string& name);
	class NodesList
	{
		pugi::xml_node _parent;
		std::string _prefix;
		size_t _counter;
		pugi::xml_node _node;
		void calcNode();
	public:
		NodesList(const pugi::xml_node& parent, const std::string& prefix) :
			_parent(parent), _prefix(prefix), _counter(0){calcNode();};
		bool finished()const{return _node.empty();};
		const pugi::xml_node& get()const;
		void next(){_counter++; calcNode();};
	};
};

class Settings
{
	pugi::xml_document _doc;
public:
	static pugi::xml_node getNode(const std::string& path, bool strict = true);
	//static pugi::xml_attribute getAttribute(const pugi::xml_node& node, const std::string& name);

	Settings(Settings const&) = delete;
	void operator=(Settings const&) = delete;
	static double get(const std::string& path, const std::string& name);
	static pugi::xml_attribute attribute(const std::string& path, const std::string& name);
	static bool exist(const std::string& path, const std::string& name = std::string());
	static double getRnd(const std::string& path);
	static double getRnd(const pugi::xml_node& node);
	//возвращает одномерную случайную величину, имеющую распределение, описанное в соответствующем узле
private:
	Settings();
};

class Rnd
{
	std::random_device _device;
	std::mt19937 _engine;
public:
	Rnd(Rnd const&) = delete;
	void operator=(Rnd const&) = delete;
	static int choose(int a = 0, int b = 1);
	static double uniform(double a = 0.0, double b = 1.0);
	static std::mt19937& engine();

private:
	Rnd():_engine(_device()){};
};

class RndVariable
{
protected:
	double _val;
	virtual void calc() = 0;
private:
	const bool _scaled;
public:
	static std::unique_ptr<RndVariable> make(const std::string& path){return make(Settings::getNode(path));};
	static std::unique_ptr<RndVariable> make(const pugi::xml_node& node);
	void init() {calc(); if(_scaled) _val = std::max(std::min(_val, 1.0), 0.0);};
	double get()const{return _val;};
	void setExternalValue(double arg) {_val = arg;};
	explicit RndVariable(bool scaled, double val = 0.0) : _val(val), _scaled(scaled){};
	virtual ~RndVariable(){};
};

class ConstantVariable final : public RndVariable
{
	void calc() override{};
public:
	ConstantVariable(double val) : RndVariable(false, val){};
};

class UniformVariable final : public RndVariable
{
	const double _a;
	const double _b;
	void calc() override{_val = Rnd::uniform(_a, _b);};
public:
	UniformVariable(bool scaled, double a = 0.0, double b = 1.0) : RndVariable(scaled), _a(a), _b(b){};
};

class NormalVariable final : public RndVariable
{
	const double _mean;
	const double _stddev;
	void calc() override{std::normal_distribution<double> dist(_mean, _stddev); _val = dist(Rnd::engine());};
public:
	NormalVariable(bool scaled, double mean, double stddev) : RndVariable(scaled), _mean(mean), _stddev(stddev){};
};

class HalfNormalVariable final : public RndVariable
{
	const double _stddev;
	void calc() override{std::normal_distribution<double> dist(0.0, _stddev); _val = std::abs(dist(Rnd::engine()));};
public:
	HalfNormalVariable(bool scaled, double stddev) : RndVariable(scaled), _stddev(stddev){};
};

class BetaVariable final : public RndVariable
{
	const double _alpha;
	const double _beta;
	void calc() override{boost::math::beta_distribution<> dist(_alpha, _beta); _val = boost::math::quantile(dist, Rnd::uniform());};
public:
	BetaVariable(bool scaled, double alpha, double beta) : RndVariable(scaled), _alpha(alpha), _beta(beta){};
};

class ParetoVariable final : public RndVariable
{
	const double _alpha;
	const double _xm;
	void calc() override{std::exponential_distribution<double> dist(_alpha); _val = exp(dist(Rnd::engine())) * _xm;};
public:
	ParetoVariable(bool scaled, double alpha, double xm) : RndVariable(scaled), _alpha(alpha), _xm(xm){};
};

double linearInterpolation(const std::pair<double, double>& a, const std::pair<double, double>& b, double x);

//class Arg
//{
//	double dist(const Arg& rhs) const;
//  double getObservation() const;
//	void setSmoothedVal(double);
//	double getWeight() const;
//}
template<class Arg>
class Squelch
{
	//см. "Genetic Algorithms for Optimization of Noisy Fitness
	//Functions and Adaptation to Changing Environments", 3.3 - 3.4
	//(Hajime Kita, Yasuhito Sano)

	struct ParamsObjective
	{
		std::vector<double> distances;
		std::vector<double> augmentations;// (F(hl) - f(x))^2
		explicit ParamsObjective(size_t pointsNum): distances(pointsNum), augmentations(pointsNum){};

		static double get(const std::vector<double> &x, std::vector<double> &grad, void *data)
		{
			//x[0] - dispersion, x[1] - distFactor
			ParamsObjective* d = reinterpret_cast<ParamsObjective*>(data);
			size_t pointsNum = d->distances.size();
			double ret = 0.0;
			for(size_t i = 0; i < pointsNum; i++)
			{
				float a = (d->distances[i] * x[1] + 1.0) * x[0];
				ret += a + (d->augmentations[i] / a);
			}
			return ret;
		}
	};
public:
	struct Params
	{
		double dispersion;
		double distFactor;
		Params(double dispers = 1.0, double distF = 1.0): dispersion(dispers), distFactor(distF){};
	};
private:
	std::vector<std::shared_ptr<const Arg> > _ungrouped;
	std::vector<std::vector<std::shared_ptr<Arg> > > _tracked;
	std::vector<double> _extDistFactors;
	std::vector<Params> _params;
	size_t _centralPointsNum;
public:
	Squelch(size_t centralPointsNum = 5) : _centralPointsNum(centralPointsNum){};
	void init(
			const std::vector<std::vector<std::shared_ptr<Arg> > >& tracked,
			const std::vector<double>& extDistFactors = {})
	{
		_extDistFactors.resize(tracked.size());
		if(extDistFactors.empty())
			std::fill(_extDistFactors.begin(), _extDistFactors.end(), 1.0);
		else
		{
			for(size_t i = 0; i < _extDistFactors.size(); i++)
			_extDistFactors[i] = extDistFactors[i % extDistFactors.size()];
		}

		_tracked = tracked;
		_params.clear();
		_params.resize(tracked.size());
		_ungrouped.clear();
		for(auto& g : _tracked)
			for(auto& p : g)
			_ungrouped.emplace_back(p);
	};
	void operator()()
	{
		for(size_t groupNum = 0; groupNum < _tracked.size(); groupNum++)
		{
			_params[groupNum] = getParams(_tracked[groupNum], _ungrouped, _params[groupNum], _centralPointsNum);
			updateSmoothedVals(groupNum);
		}
	}

	static Params getParams(
			const std::vector<std::shared_ptr<Arg> >& points,
			const std::vector<std::shared_ptr<const Arg> >& history,
			const Params& prev,
			size_t centralPointsNum)
	{
		size_t pointsNum = points.size();
		size_t historySize = history.size();
		if(!centralPointsNum)
			throw std::logic_error("Squelch::getParams: !centralPointsNum");
		if(centralPointsNum > pointsNum)
			throw std::logic_error("Squelch::getParams: centralPointsNum > pointsNum");
		ParamsObjective objectiveData(historySize);

		//ищем x
		auto iMaxU = std::max_element(points.begin(), points.end(),
				 [](const std::shared_ptr<Arg>& lhs,
					const std::shared_ptr<Arg>& rhs){return lhs->getObservation() < rhs->getObservation();});
		//ищем dl
		for(size_t i = 0; i < historySize; i++)
			objectiveData.distances[i] = (*iMaxU)->dist(*(history[i]));

		//for(size_t i = 0; i < historySize; i++)
		//	std::cout << "dist = " << objectiveData.distances[i] << "\n";

		//и f(x)
		//тут дистанции только среди points, ищем ближайшие к x
		std::vector<std::pair<double, double> > distsAndSamples(pointsNum);
		for(size_t i = 0; i < pointsNum; i++)
			distsAndSamples[i] = std::make_pair((*iMaxU)->dist(*(points[i])), points[i]->getObservation());

		//for(size_t i = 0; i < pointsNum; i++)
		//	std::cout << "dist = " << distsAndSamples[i].first <<
		//		"; sample = " << distsAndSamples[i].second << "\n";

		std::partial_sort (distsAndSamples.begin(), distsAndSamples.begin() + centralPointsNum,
				distsAndSamples.end(), [](
						const std::pair<double, double>& lhs,
						const std::pair<double, double>& rhs){return lhs.first < rhs.first;});
		double fx = 0.0;//среднее значение семплов на centralPointsNum ближайших точках
		for(size_t i = 0; i < centralPointsNum; i++)
			fx += distsAndSamples[i].second;
		fx /= static_cast<double>(centralPointsNum);

		//std::cout << "fx = " << fx << "\n";
		//и (F(hl) - f(x))^2
		for(size_t i = 0; i < historySize; i++)
			objectiveData.augmentations[i] = std::pow(history[i]->getObservation() - fx, 2.0);

		//for(size_t i = 0; i < historySize; i++)
		//	std::cout << "augm = " << objectiveData.augmentations[i] << "\n";

		std::vector<double> x = {prev.dispersion, prev.distFactor};

		nlopt::opt opt(nlopt::LN_SBPLX, 2);
		opt.set_lower_bounds(1.e-20);
		opt.set_min_objective(ParamsObjective::get, &objectiveData);

		double minf;
		nlopt::result result = opt.optimize(x, minf);
		if(result < nlopt::SUCCESS)
			throw std::runtime_error("Squelch::getParams: result < nlopt::SUCCESS");
		//std::cout << "found minimum at f(" << x[0] << "," << x[1] << ") = " << minf << std::endl;
		return Params(x[0], x[1]);
	}

	void updateSmoothedVals(size_t groupNum)
	{
		const auto& points = _tracked[groupNum];
		const auto& params = _params[groupNum];
		double extDistFactor = _extDistFactors[groupNum];

		size_t pointsNum = points.size();
		size_t historySize = _ungrouped.size();

		double avgW = 0.0;

		for(size_t i = 0; i < pointsNum; i++)
		{
			double newVal = 0.0;
			double sumW = 0.0;
			for(size_t j = 0; j < historySize; j++)
			{
				double curW =  1.0 /
						(1.0 + ((extDistFactor * params.distFactor) * points[i]->dist(*(_ungrouped[j]))));
				if(points[i] != _ungrouped[j])
					curW = std::min(curW, 0.999);
				avgW += curW;
				curW *= _ungrouped[j]->getWeight();
				newVal += _ungrouped[j]->getObservation() * curW;
				//std::cout << curW << " | ";
				sumW += curW;

			}
			if(sumW < 1.e-20)
				throw std::logic_error("Squelch::updateHistoryVals: sumW < 1.e-20");

			newVal /= sumW;
			points[i]->setSmoothedVal(newVal);
		}
		//std::cout << "; avgW = " << avgW / static_cast<double>(pointsNum * historySize) << "\n";
	}
};
#endif /* UTILS_H_ */
