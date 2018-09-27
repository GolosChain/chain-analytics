#include <algorithm>
#include <iostream>
#include <chrono>
#include "GolosEconomy.h"
#include "Utils.h"

double Strat::s_expMoving = Settings::get("squelch", "expMoving");
size_t StratPopulation::s_iterSize = Settings::attribute("population.run", "iterSize").as_uint();
double StratPopulation::s_elit = Settings::get("population.run", "elit");
double StratPopulation::s_migrationRate = Settings::get("population.run", "migrationRate");
bool Environment::s_displayEnable = Settings::attribute("display", "enable").as_bool();
double User::s_articleRatingLnFactor = Settings::get("article", "ratingLnFactor");
double User::s_articlePassesLnFactor = Settings::get("article", "passesLnFactor");
double User::s_initCharge = Settings::get("user", "charge");
double User::s_straightforwardFactorPower = Settings::get("user", "straightforwardFactorPower");
size_t User::s_maxPasses  = Settings::attribute("user", "maxPasses").as_uint();

double Article::TextProperties::dist(const Article::TextProperties& rhs)const
{
	auto& lhs = *this;
	double ret = 0.0;
	for(size_t i = 0; i < PROPERTIES_COUNT; i++)
		ret += pow(lhs[i]->get() - rhs[i]->get(), 2.0);
	return (ret > 0) ? sqrt(ret / static_cast<double>(PROPERTIES_COUNT)) : 0.0;
}

void Article::TextProperties::init()
{
	for(size_t i = 0; i < PROPERTIES_COUNT; i++)
		(*this)[i]->init();
}

Article::TextProperties::TextProperties(const std::string& attrName)
{
	for(size_t i = 0; i < PROPERTIES_COUNT; i++)
		(*this)[i] = RndVariable::make(attrName + "._" + std::to_string(i));
}

void Article::init()
{
	_rating = 0.0;
	_impactFuncSum = 0.0;
	_rewardFuncSum = 0.0;
	_votes.clear();
	_properties.init();
	_curPass = 0;
}

#ifdef VERBOSE_MODE
void Article::print(const std::string& name) const
{
	std::cout << name << ": "
	<<	"rating = " << _rating << "; "
	<<	"impactFuncSum = " << _impactFuncSum << "; "
	<<	"rewardFuncSum = " << _rewardFuncSum << "; "
	<<	"curPass = " << _curPass << "; "
	<<	"votesNum = " << _votes.size() << "\n";
}
#endif

void Article::addVote(std::shared_ptr<User>& user, double w, Rules& rules)
{
	double prevRating = _rating;
	_rating += user->getStack() * w;
	double impactDelta = rules.curatorsImpact()({{"r", _rating}}) - rules.curatorsImpact()({{"r", prevRating}});
	_impactFuncSum += impactDelta;

	_rewardFuncSum = rules.acticleReward()({{"r", _rating}});
	_votes.emplace_back(std::make_shared<Vote>(user, impactDelta));
	user->registerVote(shared_from_this(), _votes.back());
}

double Article::getCurrentReward(const GlobalProps& globalProps)const
{
	return (globalProps.rewardFuncSum < 1.e-20) ? 0.0 : (globalProps.rewardPool * _rewardFuncSum) / globalProps.rewardFuncSum;
}

double Article::cashout(const GlobalProps& globalProps)
{
	double ret = getCurrentReward(globalProps);
	if((ret < 1.e-20) || (_impactFuncSum < 1.e-20))
		return 0.0;

	for(auto& v : _votes)
		v->user.lock()->fixUtility({}, (ret * v->impact) / _impactFuncSum);
	return ret;
}

std::unique_ptr<ProjectedDataRepresentation> Environment::makeRepresentation(const std::string& path, const std::string& name, size_t rowSize)
{
	std::unique_ptr<ProjectedDataRepresentation> ret;
	if(Settings::attribute(path, "console").as_bool())
		ret = std::make_unique<MeanToConsole>();
	else
		ret = std::make_unique<GnuplotDisplay>(
				name,
				Settings::get(path, "zoomInBorder"),
				Settings::get(path, "zoomOutFactor"),
				Settings::attribute(path, "pointType").as_uint(), true, rowSize);
	return ret;
}

void  StratPopulation::addStrats(const std::string& stratInitAttrName, size_t stratsNum, size_t clan, bool reset)
{
	_strats.resize(clan + 1);
	for(size_t g = 0; g < stratsNum; g++)
		_strats[clan].emplace_back(std::make_shared<Strat>(stratInitAttrName));

	if(reset)
	{
		std::vector<double> extDistFactors;
		size_t i = 0;
		while(Settings::exist("squelch.extDistFactors", std::string("_") + std::to_string(i)))
			extDistFactors.push_back(Settings::get("squelch.extDistFactors", std::string("_") + std::to_string(i++)));
		_squelch.init(_strats, extDistFactors);
	}
}


void StratPopulation::init(const std::string& stratInitAttrName)
{

	_strats.clear();
	size_t clansNum = Settings::attribute("population.init", "clansNum").as_uint();

	for(size_t i = 0; i < clansNum; i++)
		addStrats(stratInitAttrName,
				Settings::attribute("population.init", "stratsNum").as_uint(),
				i, (i == (clansNum - 1)));
	initIterations();
}

void StratPopulation::init(const pugi::xml_node& node)
{
	_strats.clear();

	Xml::NodesList clanNodes(node, "clan_");
	while(!clanNodes.finished())
	{
		_strats.emplace_back(std::vector<std::shared_ptr<Strat> >());
		auto& clan = _strats.back();
		Xml::NodesList stratNodes(clanNodes.get(), "strat_");
		while(!stratNodes.finished())
		{
			clan.emplace_back( std::make_shared<Strat>(stratNodes.get()));
			stratNodes.next();
		}
		clanNodes.next();
	}

	std::vector<double> extDistFactors;
	size_t i = 0;
	while(Settings::exist("squelch.extDistFactors", std::string("_") + std::to_string(i)))
		extDistFactors.push_back(Settings::get("squelch.extDistFactors", std::string("_") + std::to_string(i++)));
	_squelch.init(_strats, extDistFactors);
	initIterations();
}

void StratPopulation::print(const std::string& name, std::ofstream& file)const
{
	file << "<" << name << ">\n";
	for(size_t clanN = 0; clanN < _strats.size(); clanN++)
	{
		std::string clanName = std::string("clan_") + std::to_string(clanN);
		file << "<" << clanName << ">\n";
		for(size_t groupN = 0; groupN < _strats[clanN].size(); groupN++)
			_strats[clanN][groupN]->print(std::string("strat_") + std::to_string(groupN), file);
		file << "</" << clanName << ">\n";
	}
	file << "</" << name << ">\n";
}

double Strat::getObservation(bool strict) const
{
	if(_weight > 0)
		return _expUtility;
	if(strict)
		throw std::logic_error("Population::StratGroup::getObservation");
	else return 0.0;
};

void Strat::pushObservatedUtility(double val)
{
	if(_weight > 0)
	{
		double w0 = (1.0 - s_expMoving) * _weight;
		_weight = w0 + 1.0;
		_expUtility = ((w0 * _expUtility) + val) / _weight;
	}
	else
	{
		_expUtility = val;
		_weight = 1.0;
	}
}

constexpr std::array<size_t, Strat::ACTS_COUNT> Strat::PHENOSIZES;

ProjectedDataRepresentation::PointStruct Strat::getPointStruct()
{
	ProjectedDataRepresentation::PointStruct ret;
	for(size_t phen = 0; phen < ACTS_COUNT; phen++)
	{
		std::string phenName = std::string("phen") + std::to_string(phen);
		ret.push_back({phenName, {"displ"}});

		for(size_t feature = 0; feature < PHENOSIZES[phen]; feature++)
			ret.push_back({phenName + ".f" + std::to_string(feature), {"factor", "bend"}});
	}
	return ret;
}

std::string Strat::getProbsProjName(size_t populationNum, size_t phen, const std::vector<size_t>& features)
{
	std::string popName = std::string("p_") + std::to_string(populationNum);
	std::string ret = popName + "_phen" + std::to_string(phen);
	for(size_t i = 0; i < features.size(); i++)
	{
		if(i > 0)
			ret += "/";
		ret += ".f" + std::to_string(features[i]);
	}
	return ret;
}

ProjectedDataRepresentation::PointStruct Strat::getProbsPointStruct(size_t size)
{
	ProjectedDataRepresentation::PointStruct ret;

	bool heatmap = Settings::attribute("display.probs", "heatmap").as_bool();
	for(size_t i = 0; i < size; i++)
		for(size_t phen = 0; phen < ACTS_COUNT; phen++)
		{
			if(heatmap && (PHENOSIZES[phen] > 1))
			{
				for(size_t featureX = 0; featureX < PHENOSIZES[phen]; featureX++)
					for(size_t featureY = (featureX + 1); featureY < PHENOSIZES[phen]; featureY++)
						ret.push_back({getProbsProjName(i, phen, {featureX, featureY}), {"", "", ""}});
			}
			else
			{
				std::string phenName = std::string("phen") + std::to_string(phen);
				for(size_t feature = 0; feature < PHENOSIZES[phen]; feature++)
					ret.push_back({getProbsProjName(i, phen, {feature}), {"val", "prob"}});
			}
		}

	return ret;
}

Strat::Strat()
{
	initUtility();
	for(size_t phen = 0; phen < ACTS_COUNT; phen++)
	{
		_phenotypes[phen].displ = 0.0;
		//_phenotypes[phen].post = 0.0;
		_phenotypes[phen].featureParams.resize(PHENOSIZES[phen]);
	}
}

std::pair<std::shared_ptr<Strat>, double > StratPopulation::getSphere(const std::vector<std::shared_ptr<Strat> >& clan)
{
	 std::pair<std::shared_ptr<Strat>, double > ret(Strat::getCenter(clan), 0.0);
	 for(auto& s : clan)
		 ret.second = std::max(ret.second, s->dist(*ret.first));
	 return ret;
}

std::shared_ptr<Strat> Strat::getCenter(const std::vector<std::shared_ptr<Strat> >& args)
{
	auto ret = std::make_shared<Strat>();
	if(!args.empty())
	{
		double mul = 1.0 / static_cast<double>(args.size());
		for(auto& a : args)
		{
			for(size_t phen = 0; phen < ACTS_COUNT; phen++)
			{
				ret->_phenotypes[phen].displ += a->_phenotypes[phen].displ * mul;
				//_phenotypes[phen].post;
				for(size_t feature = 0; feature < PHENOSIZES[phen]; feature++)
				{
					ret->_phenotypes[phen].featureParams[feature].setFactor(
							ret->_phenotypes[phen].featureParams[feature].factor() +
							(a->_phenotypes[phen].featureParams[feature].factor() * mul));
					ret->_phenotypes[phen].featureParams[feature].setBend(
							ret->_phenotypes[phen].featureParams[feature].bend() +
							(a->_phenotypes[phen].featureParams[feature].bend() * mul));
				}
			}
		}
	}
	return ret;
}

double Strat::getNorm()const
{
	double ret = 0.0;
	double w = 0.0;
	for(size_t phen = 0; phen < ACTS_COUNT; phen++)
	{
		ret += pow(_phenotypes[phen].displ, 2.0);
		w += 1.0;
		//ret += pow(_phenotypes[phen].post, 2.0);
		for(size_t feature = 0; feature < PHENOSIZES[phen]; feature++)
		{
			ret += pow(   _phenotypes[phen].featureParams[feature].factor(), 2.0);
			ret += pow(   _phenotypes[phen].featureParams[feature].bend(), 2.0);
			w += 2.0;
		}
	}
	return ret / w;
}

void Strat::print(const std::string& name, std::ofstream& file) const
{
	static const std::map<FeatureType, std::string> strFromFeature =
	{
			{FeatureType::PASSES_LN, "PASSES_LN"},
			{FeatureType::RATING_LN, "RATING_LN"},
			{FeatureType::TASTE_DIST, "TASTE_DIST"}
	};

	file << "<" << name << ">\n";
	for(size_t phen = 0; phen < ACTS_COUNT; phen++)
	{
		std::string phenName = std::string("phenotype_") + std::to_string(phen);
		file << "<" << phenName << ">\n";
		file << "<displ  distribution=\"constant\" val=\""<< _phenotypes[phen].displ << "\"/>\n";

		for(size_t feature = 0; feature < PHENOSIZES[phen]; feature++)
		{
			const auto& iType = strFromFeature.find(_phenotypes[phen].featureParams[feature].featureType());
			if(iType == strFromFeature.end())
				throw std::runtime_error(std::string("Strat print: can't find feature type"));

			file << "<feature_" << std::to_string(feature) <<" type=\"" << iType->second << "\">\n";
			file << "<factor distribution=\"constant\" val=\""<<
					_phenotypes[phen].featureParams[feature].factor() << "\"/>\n";
			file << "<bend  distribution=\"constant\" val=\"" <<
					_phenotypes[phen].featureParams[feature].bend() << "\"/>\n";

			file << "</feature_" << std::to_string(feature) << ">\n";
		}
		file << "</" << phenName << ">\n";
	}

	file << "</" << name << ">\n";
}

double Strat::dist(const Strat& rhs)const
{
	double ret = 0.0;
	for(size_t phen = 0; phen < ACTS_COUNT; phen++)
	{
		ret += pow(_phenotypes[phen].displ - rhs._phenotypes[phen].displ, 2.0);
		for(size_t feature = 0; feature < PHENOSIZES[phen]; feature++)
		{
			ret += pow(   _phenotypes[phen].featureParams[feature].factor()
					- rhs._phenotypes[phen].featureParams[feature].factor(), 2.0);
			ret += pow(   _phenotypes[phen].featureParams[feature].bend()
					- rhs._phenotypes[phen].featureParams[feature].bend(), 2.0);
		}
	}
	return sqrt(ret);
}

void Strat::sendTo(std::unique_ptr<ProjectedDataRepresentation>& representation, size_t color) const
{
	for(size_t phen = 0; phen < ACTS_COUNT; phen++)
	{
		std::string phenName = std::string("phen") + std::to_string(phen);
		representation->set(phenName, {_phenotypes[phen].displ, static_cast<double>(color)});

		for(size_t feature = 0; feature < PHENOSIZES[phen]; feature++)
			representation->set(phenName + ".f" + std::to_string(feature),{
					_phenotypes[phen].featureParams[feature].factor(),
					_phenotypes[phen].featureParams[feature].bend()
					,static_cast<double>(color)
					});
	}
}

double Strat::FeatureParams::calcPower(double bend)
{
	static constexpr double ln05 = log(0.5);
	return ln05 / log(sigmoid(-bend));
};

double StratPopulation::getAvgProb(std::vector<Strat::Feature> features, Strat::ActType actType)const
{
	double sumP = 0.0;
	double sumW = 0.0;
	for(auto& clan : _strats)
		for(auto& g : clan)
		{
			double curW = 1.0;//std::max(g->getObservation(false), 0.0);
			sumW += curW;
			sumP += curW * g->get(features, actType, true);

		}
	return (sumW > 0.0) ? sumP / sumW : 0.0;
}

void StratPopulation::updateProbsRepresentation(std::unique_ptr<ProjectedDataRepresentation>& representation)const
{
	bool heatmap = Settings::attribute("display.probs", "heatmap").as_bool();
	size_t pointsNum = Settings::attribute("display.probs", "pointsNum").as_uint();

	for(size_t phen = 0; phen < Strat::ACTS_COUNT; phen++)
	{
		std::vector<Strat::Feature> features(Strat::PHENOSIZES[phen]);

		if(heatmap && (Strat::PHENOSIZES[phen] > 1))
		{
			for(size_t featureX = 0; featureX < Strat::PHENOSIZES[phen]; featureX++)
				for(size_t featureY = (featureX + 1); featureY < Strat::PHENOSIZES[phen]; featureY++)
				{
					for(auto& f : features)
						f.set(0.5);

					representation->startSending();
					for(size_t i = 0; i < pointsNum; i++)
						for(size_t j = 0; j < pointsNum; j++)
						{
							double valX = static_cast<double>(i) / static_cast<double>(pointsNum - 1);
							double valY = static_cast<double>(j) / static_cast<double>(pointsNum - 1);
							features[featureX].set(valX);
							features[featureY].set(valY);
							double prob = getAvgProb(features, static_cast<Strat::ActType>(phen));
							representation->set(
									Strat::getProbsProjName(_populationNum, phen, {featureX, featureY})
							,
									{valX, valY, prob});
							representation->next();
						}
				}
		}
		else
		{

			for(size_t feature = 0; feature < Strat::PHENOSIZES[phen]; feature++)
			{
				for(auto& f : features)
					f.set(0.5);
				representation->startSending();
				for(size_t i = 0; i < pointsNum; i++)
				{
					double val = static_cast<double>(i) / static_cast<double>(pointsNum - 1);
					features[feature].set(val);
					double prob = getAvgProb(features, static_cast<Strat::ActType>(phen));
					representation->set( Strat::getProbsProjName(_populationNum, phen, {feature}),{val, prob});
					representation->next();
				}
			}
		}
	}
}

void Strat::init(const pugi::xml_node& node)
{
	initUtility();
	static const std::map<std::string, FeatureType> featureFromStr =
	{
		{"PASSES_LN", FeatureType::PASSES_LN},
		{"RATING_LN",     FeatureType::RATING_LN},
		{"TASTE_DIST",     FeatureType::TASTE_DIST}
	};//бррр... %(

	size_t i = 0;
	Xml::NodesList phenotypeNodes(node, "phenotype_");
	while(!phenotypeNodes.finished())
	{
		if(i >= ACTS_COUNT)
			throw std::runtime_error(std::string("Strat constructor: phenotypes list overflow"));
		_phenotypes[i].displ = Settings::getRnd(Xml::getNode(phenotypeNodes.get(), "displ"));
		size_t j = 0;
		Xml::NodesList featureNodes(phenotypeNodes.get(), "feature_");
		while(!featureNodes.finished())
		{
			if(j >= PHENOSIZES[i])
				throw std::runtime_error(std::string("Strat constructor: features list overflow"));
			std::string typeStr = Xml::getAttribute(featureNodes.get(), "type").as_string();
			const auto& iType = featureFromStr.find(typeStr);
			if(iType == featureFromStr.end())
				throw std::runtime_error(std::string("Strat constructor: can't find feature type <") + typeStr + ">");
			_phenotypes[i].featureParams.emplace_back(FeatureParams(iType->second,
						Settings::getRnd(Xml::getNode(featureNodes.get(), "factor")),
						Settings::getRnd(Xml::getNode(featureNodes.get(), "bend"))));
			featureNodes.next();
			++j;
		}
		if(j != PHENOSIZES[i])
			throw std::runtime_error(std::string("Strat constructor: features is not enough"));
		phenotypeNodes.next();
		++i;
	}
	if(i != ACTS_COUNT)
		throw std::runtime_error(std::string("Strat constructor: phenotypes is not enough"));

}

Strat::Strat(const pugi::xml_node& node)
{
	init(node);
}

Strat::Strat(const std::string& initAttrName) : _initAttrName(initAttrName)
{
	pugi::xml_node node = Settings::getNode(initAttrName);
	init(node);
}

double Strat::get(const std::vector<Feature>& features, ActType actType, bool disableFeatureTypeCheck) const
{
	size_t actTypeNum = static_cast<size_t>(actType);
	if(actTypeNum >= ACTS_COUNT)
		throw std::logic_error("Strat::getProb: wrong act type");

	size_t phenosize = PHENOSIZES[actTypeNum];
	auto& curPhenotypes =  _phenotypes[actTypeNum];

	if(curPhenotypes.featureParams.size() != phenosize)
		throw std::logic_error("Strat::getProb: wrong size of phenotypes");
	if(features.size() != phenosize)
		throw std::logic_error("Strat::getProb: wrong size of features vector");
	double ret = curPhenotypes.displ;
	for(size_t i = 0; i < phenosize; i++)
	{
		if(!disableFeatureTypeCheck && !features[i].checkType(curPhenotypes.featureParams[i]._featureType))
			throw std::logic_error("Strat::getProb: wrong feature type");
		ret += pow(features[i].get(), curPhenotypes.featureParams[i].power()) * curPhenotypes.featureParams[i].factor();
	}

	return  activation(ret);//>0
};

double Strat::mix(double lhs, double rhs, bool pnx)
{
	double limit = Settings::get("strat.breed", "limit");
	double d = std::abs(lhs - rhs);
	if(pnx)
	{
		std::normal_distribution<double> dist(lhs, d * Settings::get("strat.breed", "normalD"));
		return std::max(std::min(dist(Rnd::engine()), limit), -limit);
	}
	else
	{
		d *= Settings::get("strat.breed", "uniformD");
		return Rnd::uniform(std::max(lhs - d, -limit), std::min(lhs + d, limit));
	}
}

void Strat::born(const Strat& parentA, const Strat& parentB)
{
	bool pnx = (Rnd::uniform() < Settings::get("strat.breed", "pnxProb"));
	for(size_t phen = 0; phen < ACTS_COUNT; phen++)
	{
		_phenotypes[phen].displ = mix(parentA._phenotypes[phen].displ, parentB._phenotypes[phen].displ, pnx);

		for(size_t i = 0; i < PHENOSIZES[phen]; i++)
		{
			_phenotypes[phen].featureParams[i].setFactor(mix(
						parentA._phenotypes[phen].featureParams[i].factor(),
						parentB._phenotypes[phen].featureParams[i].factor(), pnx));

			_phenotypes[phen].featureParams[i].setBend(mix(
						parentA._phenotypes[phen].featureParams[i].bend(),
						parentB._phenotypes[phen].featureParams[i].bend(), pnx));
		}
	}
	initUtility();
}

double User::getVoteWeight(const std::shared_ptr<Article>& article)
{
	double dist = _taste.dist(article->getProperties());
	double ret = 0.0;
	if(static_cast<bool>(_strat))
	{
		std::vector<Strat::Feature> features =
		{
			Strat::Feature(Strat::FeatureType::TASTE_DIST),
			Strat::Feature(Strat::FeatureType::RATING_LN)
		};
		features[0].set(dist);
		features[1].set(s_articleRatingLnFactor * log((1.0 + article->getRating())));
		ret = std::min(_strat->get(features, Strat::ActType::BET_WEIGHT), _charge);
	}
	else
		ret = pow(1.0 - dist, s_straightforwardFactorPower);

	_charge -= ret;
	return ret;

}

std::shared_ptr<Article> User::pickArticle(std::vector<std::shared_ptr<Article> >& articles, std::vector<double>& buf) const
{
	if(static_cast<bool>(_strat))
	{
		if(buf.size() != articles.size())
			throw std::logic_error("User::pickArticle buf.size() != articles.size()");

		std::vector<Strat::Feature> features =
		{
			Strat::Feature(Strat::FeatureType::PASSES_LN),
			Strat::Feature(Strat::FeatureType::RATING_LN)
		};

		double sumW = 0.0;
		for(size_t i = 0; i < articles.size(); i++)
		{
			features[0].set(s_articlePassesLnFactor * log((1.0 + static_cast<double>(articles[i]->getPasses()))));
			features[1].set(s_articleRatingLnFactor * log((1.0 + articles[i]->getRating())));
			double curW = _strat->get(features, Strat::ActType::PICK_WEIGHT);
			buf[i] = curW;
			sumW += curW;
		}
		if(sumW > 1.0)
			for(auto& w : buf)
				w /= sumW;

		double rnd = Rnd::uniform();
		sumW = 0.0;
		for(size_t i = 0; i < articles.size(); i++)
		{
			sumW += buf[i];
			if(rnd <= sumW)
				return articles[i];
		}
		return std::shared_ptr<Article>();
	}
	else
		return articles[Rnd::choose(0, articles.size() - 1)];
}

User::User() :
		_charge(s_initCharge),
		_stack(RndVariable::make("user.stack")),
		_taste("user.taste"),
		_fixedUtility(0.0), _curPass(s_maxPasses) {}

double User::getTotalUtility(const GlobalProps& globalProps)const
{
	double ret = _fixedUtility;

	for(auto& v : _votes)
		if(v.first.expired())
			throw std::logic_error("User::getTotalUtility article's pointer is expired");
		else if(!v.second.expired())
		{
			auto article = v.first.lock();
			auto vote = v.second.lock();
			if(vote->impact > 1.e-20)
				ret += (article->getCurrentReward(globalProps) * vote->impact)
							/ std::max(article->getImpactFuncSum(), vote->impact);
		}

	return ret;
}

void User::startPass(StratEnvironment& strats, const Rules& rules, const GlobalProps& globalProps)
{
	if(((_curPass++) >= s_maxPasses) || (_charge < 0.001))
	{
		if(static_cast<bool>(_strat))
			_strat->pushObservatedUtility(getTotalUtility(globalProps) / _stack->get());
		_charge = s_initCharge;
		_stack->init();
		_taste.init();
		_fixedUtility = 0.0;
		_curPass = 0;
		_votes.clear();

		bool straightforward = (Rnd::uniform() < rules.getStraightforwardProb());
		_strat = straightforward ? std::shared_ptr<Strat>() : strats.pick(_stack->get(), 0.5);
		_stack->setExternalValue(strats.fixStackSize(_stack->get()));
	}
}

#ifdef VERBOSE_MODE
void User::print(const GlobalProps& globalProps, const std::string& name) const
{
	size_t aliveVotes = 0;

	for(auto& v : _votes)
		if(!v.second.expired())
			++aliveVotes;

	std::cout << name << ": "
	<<  "str8forward = " << !static_cast<bool>(_strat) << "; "
	<<	"charge = " << _charge << "; "
	<<	"stack = " << _stack->get() << "; "
	<<	"fixedUtility = " << _fixedUtility << "; "
	<<	"totalUtility = " << getTotalUtility(globalProps) << "; "
	<<	"curPass = " << _curPass << "; "
	<<	"votesNum = " << _votes.size() << "; "
	<<	"aliveVotes = " << aliveVotes << "\n";
}
#endif

void User::registerVote(const std::shared_ptr<Article>& article, const std::shared_ptr<Vote>& vote)
{
	_votes.push_back(std::make_pair(std::weak_ptr<Article>(article), std::weak_ptr<Vote>(vote)));
}

std::unique_ptr<Func::Operation> Func::Operation::make(const std::string& path)
{
	std::string name(Settings::attribute(path, "operaton").as_string());
	if(name == "pow")
		return std::make_unique<Pow>(Settings::get(path, "p"));
	else if(name == "abs")
		return std::make_unique<Abs>();
	else if(name == "const")
		return std::make_unique<Const>(Settings::get(path, "a"));
	else if(name == "push")
		return std::make_unique<Push>(Settings::attribute(path, "arg").as_string());
	else if(name == "dup")
		return std::make_unique<Dup>();
	else if(name == "add")
		return std::make_unique<Add>();
	else if(name == "sub")
		return std::make_unique<Sub>();
	else if(name == "mul")
		return std::make_unique<Mul>();
	else if(name == "div")
		return std::make_unique<Div>();
	else if(name == "swap")
		return std::make_unique<Swap>();
	throw std::runtime_error(std::string("Func::Operation::make: unknown operation"));
}

void Func::Pow::operator()(Condition& s)const
{
	if(s._stack.empty())
		throw std::runtime_error(std::string("Func::Pow::operator(): stack is empty"));
	s._stack.top() = std::pow(s._stack.top(), _p);
}

void Func::Abs::operator()(Condition& s)const
{
	if(s._stack.empty())
		throw std::runtime_error(std::string("Func::Abs::operator(): stack is empty"));
	s._stack.top() = std::abs(s._stack.top());
}

void Func::Const::operator()(Condition& s)const
{
	s._stack.push(_a);
}

void Func::Push::operator()(Condition& s)const
{
	auto i = s._args.find(_argName);
	if(i != s._args.end())
		s._stack.push(i->second);
	else
		throw std::runtime_error(std::string("Func::Push::operator(): arg doesn't exist: ") + _argName);

}

void Func::Dup::operator()(Condition& s)const
{
	if(s._stack.empty())
		throw std::runtime_error(std::string("Func::Dup::operator(): stack is empty"));
	s._stack.push(s._stack.top());
}

std::pair<double, double> Func::Binary::pop(Condition& s)const
{
	std::pair<double, double> ret;
	if(s._stack.empty())
		throw std::runtime_error(std::string("Func::Binary::pop::operator(): stack is empty"));
	ret.first = s._stack.top();
	s._stack.pop();
	if(s._stack.empty())
		throw std::runtime_error(std::string("Func::Binary::pop::operator(): stack contain just one element"));
	ret.second = s._stack.top();
	s._stack.pop();
	return ret;
}

void Func::Add::operator()(Condition& s)const
{
	auto args = pop(s);
	s._stack.push(args.first + args.second);
}

void Func::Sub::operator()(Condition& s)const
{
	auto args = pop(s);
	s._stack.push(args.first - args.second);
}

void Func::Mul::operator()(Condition& s)const
{
	auto args = pop(s);
	s._stack.push(args.first * args.second);
}

void Func::Div::operator()(Condition& s)const
{
	auto args = pop(s);
	s._stack.push(args.first / args.second);
}

void Func::Swap::operator()(Condition& s)const
{
	auto args = pop(s);
	s._stack.push(args.first);
	s._stack.push(args.second);
}

Func::Func(const std::string& path)
{
	if(!Settings::exist(path))
		throw std::runtime_error(std::string("Func::Func path doesn't exits: ") + path);
	{
		size_t i = 0;
		std::string opPath(path + "._" + std::to_string(i));
		while(Settings::exist(opPath))
		{
			_operations.emplace_back(Operation::make(opPath));
			opPath = path + "._" + std::to_string(++i);
		}
	}
}

double Func::operator()(std::map<std::string, double>&& input) const
{
	while(!_condition._stack.empty())
		_condition._stack.pop();
	_condition._args = std::move(input);

	for(auto& f : _operations)
			(*f)(_condition);
	if(_condition._stack.empty())
		throw std::runtime_error(std::string("Func::operator(): stack is empty"));

	return _condition._stack.top();
}

Rules::Rules(const std::string& path) :
	_curatorsImpact(path + ".curatorsImpact"),
	_acticleReward(path + ".acticleReward"),
	_straightforwardProb(Settings::get(path, "straightforwardProb")){};


//////////////////////////////////////////
std::shared_ptr<Strat>& StratPopulation::pick()
{
	if((_index.first < _strats.size()) && (_index.second >= _strats[_index.first].size()))
	{
		++_index.first;
		_index.second = 0;
	}
	if(_index.first >= _strats.size())
	{
		++_iteration;
		if(_iteration > s_iterSize)
		{
			evolutionStep();
			_iteration = 0;
		}
		for(auto& clan : _strats)
			std::random_shuffle(clan.begin(), clan.end());
		_index.first = 0;
		_index.second = 0;
	}
	return _strats[_index.first][_index.second++];
}

void StratPopulation::sendTo(std::unique_ptr<ProjectedDataRepresentation>& representation, size_t color) const
{
	for(auto& clan : _strats)
		for(auto& strat : clan)
		{
			strat->sendTo(representation, color);
			representation->next();
		}
}

void StratEnvironment::sendTo(std::unique_ptr<ProjectedDataRepresentation>& representation)const
{
	representation->startSending();
	for(size_t i = 0; i < _populations.size(); i++)
		_populations[i]->sendTo(representation, i);
}

void StratEnvironment::updateProbsRepresentation(std::unique_ptr<ProjectedDataRepresentation>& representation)const
{
	for(auto& p :_populations)
		p->updateProbsRepresentation(representation);
}

void StratPopulation::evolutionStep()
{
	_squelch();
	size_t clanN = 0;
	for(auto& clan : _strats)
	{
		std::sort(clan.begin(), clan.end(),[](const std::shared_ptr<Strat>& lhs, const std::shared_ptr<Strat>& rhs){
			return lhs->getSmoothedUtility() < rhs->getSmoothedUtility();});

		size_t stratsNum = clan.size();
		size_t firstSurv = static_cast<size_t>(static_cast<double>(stratsNum) * (1.0 - s_elit));
		for(size_t curStrat = 0; curStrat < firstSurv; curStrat++)
			clan[curStrat]->born(*clan[Rnd::choose(firstSurv, stratsNum - 1)], *clan[Rnd::choose(firstSurv, stratsNum - 1)]);

		++clanN;
	}
	std::vector<std::pair<std::shared_ptr<Strat>, double > > spheres;
	for(auto& clan : _strats)
		spheres.emplace_back(getSphere(clan));
	for(size_t clanN = 0; clanN < _strats.size(); clanN++)
	{
		auto& clanA = _strats[clanN];
		size_t clanNb = (clanN + 1) % _strats.size();
		auto& clanB =  _strats[clanNb];
		double clansDist = std::max((spheres[clanN].first->dist(*(spheres[clanNb].first)))
				- (spheres[clanN].second + spheres[clanNb].second), 0.0);

		double migrationProb = _migrationProb({{"d", clansDist}});
		double migrationSize = std::ceil(static_cast<double>(std::min(clanA.size(), clanB.size())) * s_migrationRate);
		if(Rnd::uniform() < migrationProb)
			for(size_t m = 0; m < migrationSize; m++)
				std::swap(clanA[Rnd::choose(0, clanA.size() - 1)], clanB[Rnd::choose(0, clanB.size() - 1)]);
	}
}



void StratEnvironment::print(std::ofstream& file)const
{
	file << "<"  << "stratEnvironment" << ">\n";
	for(size_t i = 0; i < _populations.size(); i++)
		_populations[i]->print(std::string("population_") +  std::to_string(i), file);
	file << "</" << "stratEnvironment" << ">\n";
}

void Environment::save()const
{
	std::ofstream populationsInfo;
	populationsInfo.open (_resultFileName);
	populationsInfo << "<?xml version=\"1.0\"?>\n";
	_strats.print(populationsInfo);
	populationsInfo.close();
}

#ifdef VERBOSE_MODE
void Environment::print(std::shared_ptr<User>& user, const std::shared_ptr<Article>& article, const std::string& name)const
{
	std::cout << "============================\n"<< name << "\n";
	_globalProps.print();
	if(static_cast<bool>(user))
		user->print(_globalProps, "user");
	if(static_cast<bool>(article))
		article->print("article");
	std::cout << "\n";
}
#endif

void Environment::run(const std::string& rulesAttrPath)
{
	Rules rules(rulesAttrPath);
	size_t articlesNum = Settings::attribute("environment", "articlesNum").as_uint();
	size_t usersNum = Settings::attribute("environment", "usersNum").as_uint();
	size_t passesNum = Settings::attribute("environment", "passesNum").as_uint();
	size_t displayPeriod = Settings::attribute("display", "period").as_uint();
	size_t reportPeriod = Settings::attribute("report", "period").as_uint();
	size_t articlesPeriod = Settings::attribute("environment", "articlesPeriod").as_uint();

	std::vector<std::shared_ptr<Article> > articles(articlesNum);
	for(auto& a : articles)
		a = std::make_shared<Article>();
	auto curArticle = articles.begin();
	std::vector<std::shared_ptr<User> > users(usersNum);
	for(auto& u : users)
		u = std::make_shared<User>();
	auto curUser = users.begin();

	std::vector<double> bufArticlesWeights(articles.size());
	std::chrono::milliseconds startTime = std::chrono::duration_cast< std::chrono::milliseconds >
			(std::chrono::system_clock::now().time_since_epoch());

	for(size_t pass = 0; pass < passesNum; pass++)
	{

		(*curUser)->startPass(_strats, rules, _globalProps);
		auto pickedArticle = (*curUser)->pickArticle(articles, bufArticlesWeights);

#ifdef VERBOSE_MODE
		if(s_displayEnable && ((pass % displayPeriod) == 0))
			print(*curUser, pickedArticle, "START PASS");
#endif
		_globalProps.rewardPool += 1.0;
		if(static_cast<bool>(pickedArticle))
		{
			double prevRewardSum = pickedArticle->getRewardFuncSum();
			double woteWeight = (*curUser)->getVoteWeight(pickedArticle);
			pickedArticle->addVote(*curUser, woteWeight, rules);
			double newRewardSum = pickedArticle->getRewardFuncSum();

			if(newRewardSum < prevRewardSum)
				throw std::logic_error("newRewardSum < prevRewardSum");

			_globalProps.rewardFuncSum += (newRewardSum - prevRewardSum);
		}

		if((pass % articlesPeriod) == 0)
		{
			_globalProps.rewardPool -= (*curArticle)->cashout(_globalProps);
			_globalProps.rewardFuncSum -= (*curArticle)->getRewardFuncSum();

			(*curArticle)->init();

			if((++curArticle) == articles.end())
				curArticle = articles.begin();
		}

		if((_globalProps.rewardPool < 0.0) || (_globalProps.rewardFuncSum < 0.0))
			throw std::logic_error("((_globalProps.rewardPool < 0.0) || (_globalProps.rewardFuncSum < 0.0))");


#ifdef VERBOSE_MODE
		if(s_displayEnable && ((pass % displayPeriod) == 0))
			print(*curUser, pickedArticle, "FINISH PASS");
#endif

		if((++curUser) == users.end())
			curUser = users.begin();

		for(auto& a : articles)
			a->pass();

		if(s_displayEnable && ((pass % displayPeriod) == 0))
		{
			std::cout << "pass = " << pass << "\n";

			std::chrono::milliseconds finishTime = std::chrono::duration_cast< std::chrono::milliseconds >
					(std::chrono::system_clock::now().time_since_epoch());
			std::cout << "time = " << (finishTime - startTime).count() << "\n";

			if(_stratRepresentation)
			{
				_strats.sendTo(_stratRepresentation);
				_stratRepresentation->show();
			}
			if(_probsRepresentation)
			{
				_strats.updateProbsRepresentation(_probsRepresentation);
				_probsRepresentation->show();
			}
		}

		if(pass && (pass % reportPeriod) == 0)
			save();
	}
}

StratEnvironment::StratEnvironment(const std::string& src, bool loadFromFile)
{
	size_t i = 0;
	_minStackSize = Settings::get("user.stackGroupBorders", "min");
	while(Settings::exist("user.stackGroupBorders", std::string("_") + std::to_string(i)))
		_stackBorders.emplace_back(Settings::get("user.stackGroupBorders", std::string("_") + std::to_string(i++)));
	i = 0;

	_populations.resize(_stackBorders.size() + 1);
	size_t n = 0;

	for(auto& population : _populations)
		population = std::make_unique<StratPopulation>(n++);

	if(loadFromFile)
	{
		pugi::xml_document doc;
		if (!doc.load_file(src.c_str()))
			throw std::runtime_error(std::string("can't load file: ") + src);
		pugi::xml_node parent = doc.first_child();

		Xml::NodesList populationNodes(parent, "population_");
		auto iPopulation = _populations.begin();
		n = 0;
		while((!populationNodes.finished()) && (iPopulation != _populations.end()))
		{
			(*iPopulation)->init(populationNodes.get());
			iPopulation++;
			populationNodes.next();
			n++;
		}

		if((!populationNodes.finished()) || (iPopulation != _populations.end()))
			throw std::runtime_error(std::string("StratEnvironment::StratEnvironment: wrong populations size"));

	}
	else
		for(auto& population : _populations)
			population->init(src);

}

double StratEnvironment::fixStackSize(double stack) const
{
	size_t i = getUserType(stack);
	return(!i) ? _minStackSize : _stackBorders[i - 1];
}

size_t StratEnvironment::getUserType(double stack) const
{
	size_t i = 0;
	while((i < _stackBorders.size()) && (stack > _stackBorders[i]))
		i++;
	return i;
}

Environment::Environment(const std::string& resultFileName, const std::string& srcFileName):
		_strats(srcFileName.empty() ? "strat.init" : srcFileName, !srcFileName.empty()),
		_resultFileName(resultFileName),
		_globalProps{0.0, 0.0}
{
	if(Settings::attribute("display", "enable").as_bool())
	{
		_stratRepresentation = makeRepresentation("display.strat", "");
		_stratRepresentation->init(Strat::getPointStruct(), true);

		_probsRepresentation = makeRepresentation("display.probs", "", _strats.getStackGroupsNum() * Strat::ACTS_COUNT);
		_probsRepresentation->init(Strat::getProbsPointStruct(_strats.size()), false);
	}
}

