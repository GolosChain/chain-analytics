#ifndef GOLOSECONOMY_H_
#define GOLOSECONOMY_H_
#include <list>
#include <memory>
#include <vector>
#include <set>
#include <array>
#include <map>
#include <cmath>
#include <string>
#include <stack>
#include <iostream>
#include <fstream>
#include "Utils.h"
#include "DataRepresentation.h"
//#define VERBOSE_MODE

class User;
class Article;

struct GlobalProps
{
	double rewardPool = 0.0;
	double rewardFuncSum = 0.0;
#ifdef VERBOSE_MODE
	void print()const { std::cout << "grobalProps: pool = " << rewardPool << ", funcSum = " << rewardFuncSum << "\n"; };
#endif
};

class Func
{
	struct Condition
	{
		std::stack<double> _stack;
		std::map<std::string, double> _args;
	};

	class Operation
	{
	public:
		virtual void operator()(Condition& s)const = 0;
		virtual ~Operation(){};
		static std::unique_ptr<Operation> make(const std::string& path);
	};

	class Pow : public Operation
	{
		double _p;
	public:
		Pow(double p) : _p(p){};
		void operator()(Condition& s)const override;
	};

	class Abs : public Operation
	{
	public:
		void operator()(Condition& s)const override;
	};

	class Const : public Operation
	{
		double _a;
	public:
		Const(double a) : _a(a){};
		void operator()(Condition& s)const override;
	};

	class Push : public Operation
	{
		std::string _argName;
	public:
		Push(const std::string& argName) : _argName(argName){};
		void operator()(Condition& s)const override;
	};

	class Dup : public Operation
	{
	public:
		void operator()(Condition& s)const override;
	};

	class Binary : public Operation
	{
	protected:
		std::pair<double, double> pop(Condition& s)const;
	};

	class Add : public Binary
	{
	public:
		void operator()(Condition& s)const override;
	};

	class Sub : public Binary
	{
	public:
		void operator()(Condition& s)const override;
	};

	class Mul : public Binary
	{
	public:
		void operator()(Condition& s)const override;
	};

	class Div: public Binary
	{
	public:
		void operator()(Condition& s)const override;
	};

	class Swap: public Binary
	{
	public:
		void operator()(Condition& s)const override;
	};

	mutable Condition _condition;
	std::vector<std::unique_ptr<Operation> > _operations;
public:
	Func(const std::string& path);
	double operator()(std::map<std::string, double>&& input) const;
};

class Rules final
{
	Func _curatorsImpact;
	Func _acticleReward;
	double _straightforwardProb;
public:
	Rules(const std::string& path);
	const Func& curatorsImpact()const {return _curatorsImpact;};
	const Func& acticleReward()const  {return _acticleReward;};
	double getStraightforwardProb()const {return _straightforwardProb;};
};

struct Vote
{
	const std::weak_ptr<User> user;
	double impact;
	Vote(const std::shared_ptr<User>& u, double i): user(u), impact(i) {};
};

class Article final : public std::enable_shared_from_this<Article>
{
public:
	static constexpr size_t PROPERTIES_COUNT = 1;

	class Key{friend class Article; Key(){};};
	class TextProperties : public std::array<std::unique_ptr<RndVariable>, PROPERTIES_COUNT>
	{
	public:
		double dist(const TextProperties& rhs) const;
		TextProperties(const std::string& attrName);
		void init();
	};

private:
	TextProperties _properties;
	double _rating;
	double _impactFuncSum;
	double _rewardFuncSum;
	std::list<std::shared_ptr<Vote> > _votes;
	size_t _curPass;

public:
	Article(const std::string& attrName = "article"):_properties(attrName + ".properties") {init();};
	double getRewardFuncSum()const {return _rewardFuncSum;};
	double getImpactFuncSum()const {return _impactFuncSum;};
	void init();
	void pass() { _curPass++; };
	size_t getPasses()const {return _curPass;};
	double getRating()const {return _rating;};
	double getCurrentReward(const GlobalProps& globalProps)const;
	double cashout(const GlobalProps& globalProps);
	void addVote(std::shared_ptr<User>& user, double w, Rules& rules);
	const TextProperties& getProperties()const {return _properties;};
#ifdef VERBOSE_MODE
	void print(const std::string& name)const;
#endif
};

class Strat final
{
	static double sigmoid(double arg) {return 1.0 / (1.0 + std::exp(-arg));};
public:
	enum class ActType{PICK_WEIGHT, BET_WEIGHT, count};
	static constexpr size_t ACTS_COUNT = static_cast<size_t>(ActType::count);
	static constexpr std::array<size_t, ACTS_COUNT> PHENOSIZES = {2, 2};//lengths of phenotypes
	static ProjectedDataRepresentation::PointStruct getPointStruct();
	static ProjectedDataRepresentation::PointStruct getProbsPointStruct(size_t size);
	static std::string getProbsProjName(size_t populationNum, size_t phen, const std::vector<size_t>& features);
	enum class FeatureType{PASSES_LN, RATING_LN, TASTE_DIST, UNDEF};
	class Feature
	{
		const FeatureType _featureType;
		double _val;
	public:
		Feature(FeatureType t = FeatureType::UNDEF): _featureType(t), _val(0.0){};
		double get()const {return _val;};
		void set(double v) {_val = std::max(std::min(v, 1.0), 0.0);};
		bool checkType(FeatureType t)const {return (_featureType == t);};
	};

private:
	struct FeatureParams
	{
		const FeatureType _featureType;
	private:
		double _factor;
		double _bend;
		double _power;

	static double calcPower(double bend);
	public:
		FeatureParams(): _featureType(FeatureType::UNDEF), _factor(0.0), _bend(0.0), _power(calcPower(0.0)){};
		FeatureParams(FeatureType t, double f = 0.0, double b = 0.0)
			: _featureType(t), _factor(f), _bend(b), _power(calcPower(b)){};
		double factor()const {return _factor;};
		double bend()const {return _bend;};
		double power()const {return _power;};
		FeatureType featureType()const{return _featureType;};
		void setFactor(double arg) {_factor = arg;};
		void setBend(double arg) {_bend = arg; _power = calcPower(arg);};
	};
	struct Phenotype
	{
		std::vector<FeatureParams> featureParams;
		double displ;
	};

	std::array<Phenotype, ACTS_COUNT> _phenotypes;
	static double activation(double arg) {return sigmoid(arg);};
	std::string _initAttrName;
	static double mix(double lhs, double rhs, bool pnx = true);
	void init(const pugi::xml_node& node);

//---utility stuff---
	static double s_expMoving;
	double _weight;
	double _expUtility;
	double _smoothedUtility;
public:
	void initUtility() {_weight = 0.0; _expUtility = 0.0; _smoothedUtility = 0.0;};
	void pushObservatedUtility(double val);
	void setSmoothedVal(double arg) {_smoothedUtility = arg;};
	double getObservation(bool strict = true) const;
	double getWeight()const{return _weight;};
	double getSmoothedUtility()const {return _smoothedUtility;};

public:
	Strat();
	Strat(const std::string& initAttrName);
	Strat(const pugi::xml_node& node);
	const std::string& getInitAttrName()const {return _initAttrName;};
	void born(const Strat& parentA, const Strat& parentB);

	double get(const std::vector<Feature>& features, ActType actType, bool disableFeatureTypeCheck = false) const;

	void sendTo(std::unique_ptr<ProjectedDataRepresentation>& representation, size_t color) const;

	double dist(const Strat& rhs) const;
	static std::shared_ptr<Strat> getCenter(const std::vector<std::shared_ptr<Strat> >& args);
	double getNorm()const;

	void print(const std::string& name, std::ofstream& file) const;
};


class StratPopulation final
{
	static size_t s_iterSize;
	size_t _populationNum;
	static double s_elit;
	static double s_migrationRate;
	Func _migrationProb;
	std::vector<std::vector<std::shared_ptr<Strat> > > _strats;
	Squelch<Strat> _squelch;
	std::pair<size_t, size_t> _index;
	size_t _iteration;

	void initIterations(){_index.first = _strats.size(); _index.second = 0; _iteration = 0;};
	double getAvgProb(std::vector<Strat::Feature> features, Strat::ActType actType)const;
	void evolutionStep();
	void addStrats(const std::string& stratInitAttrName, size_t stratsNum, size_t clan, bool reset);
	std::pair<std::shared_ptr<Strat>, double > getSphere(const std::vector<std::shared_ptr<Strat> >& clan);

public:
	StratPopulation(size_t n): _populationNum(n),
		 _migrationProb("population.migrationProb"),
		 _squelch(Settings::attribute("squelch", "centralPointsNum").as_uint()), _iteration(0){};
	void init(const std::string& stratInitAttrName);
	void init(const pugi::xml_node& node);
	void print(const std::string& name, std::ofstream& file)const;
	std::shared_ptr<Strat>& pick();

	void sendTo(std::unique_ptr<ProjectedDataRepresentation>& representation, size_t color) const;
	void updateProbsRepresentation(std::unique_ptr<ProjectedDataRepresentation>& representation)const;
};

class StratEnvironment
{
	std::vector<double> _stackBorders;
	double _minStackSize;

	std::vector<std::unique_ptr<StratPopulation> > _populations;
	size_t getUserType(double stack) const;
public:
	StratEnvironment(const std::string& src, bool loadFromFile);
	std::shared_ptr<Strat>& pick(double stack, double skill){return _populations[getUserType(stack)]->pick();};
	void sendTo(std::unique_ptr<ProjectedDataRepresentation>& representation)const;
	void updateProbsRepresentation(std::unique_ptr<ProjectedDataRepresentation>& representation)const;
	size_t size()const{return _populations.size();};
	void print(std::ofstream& file)const;
	size_t getStackGroupsNum()const{return (_stackBorders.size() + 1);};
	double fixStackSize(double val) const;
};

class User final
{
	static size_t s_maxPasses;
	static double s_articleRatingLnFactor;
	static double s_articlePassesLnFactor;
	static double s_initCharge;
	static double s_straightforwardFactorPower;
	double getTotalUtility(const GlobalProps& globalProps)const;
	double _charge;
	std::unique_ptr<RndVariable> _stack;
	Article::TextProperties _taste;
	double _fixedUtility;
	std::shared_ptr<Strat> _strat;
	std::list<std::pair<std::weak_ptr<Article>, std::weak_ptr<Vote> > > _votes;
	size_t _curPass;

public:
	User();
	std::shared_ptr<Article> pickArticle(std::vector<std::shared_ptr<Article> >& articles, std::vector<double>& buf) const;
	double getVoteWeight(const std::shared_ptr<Article>& article); //_charge is changing here
	void registerVote(const std::shared_ptr<Article>& article, const std::shared_ptr<Vote>& vote);
	void fixUtility(Article::Key, double arg) {_fixedUtility += arg;};
	double getStack()const { return _stack->get(); };
	void startPass(StratEnvironment& strats, const Rules& rules, const GlobalProps& globalProps);
#ifdef VERBOSE_MODE
	void print(const GlobalProps& globalProps, const std::string& name)const;
#endif
};

class Environment final
{
	static bool s_displayEnable;
	StratEnvironment _strats;
	std::string _resultFileName;
	GlobalProps _globalProps;
	static std::unique_ptr<ProjectedDataRepresentation> makeRepresentation(const std::string& path, const std::string& name, size_t rowSize = 0);
	std::unique_ptr<ProjectedDataRepresentation> _stratRepresentation;
	std::unique_ptr<ProjectedDataRepresentation> _probsRepresentation;
	void save()const;
#ifdef VERBOSE_MODE
	void print(std::shared_ptr<User>& user, const std::shared_ptr<Article>& article, const std::string& name)const;
#endif
public:
	Environment(const std::string& resultFileName, const std::string& srcFileName = std::string());
	void run(const std::string& rulesAttrPath);
};


#endif /* GOLOSECONOMY_H_ */
