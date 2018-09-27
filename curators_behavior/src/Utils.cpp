
#include "Utils.h"

const pugi::xml_node& Xml::NodesList::get()const
{
	if(_node.empty())
		throw std::runtime_error("Xml::NodeList::get _node is empty");
	return _node;
}

void Xml::NodesList::calcNode()
{
	_node = Xml::getNode(_parent, _prefix + std::to_string(_counter), false);
}

int Rnd::choose(int a, int b)
{
	std::uniform_int_distribution<int> dist(a, b);
	return dist(engine());
}

double Rnd::uniform(double a, double b)
{
	std::uniform_real_distribution<double> dist(a, b);
	return dist(engine());
}

std::mt19937& Rnd::engine()
{
	static Rnd instance;
	return instance._engine;
}

Settings::Settings()
{
	if (!_doc.load_file("Settings.xml"))
		throw std::runtime_error("Settings: can't load file");
}

pugi::xml_node Xml::getNode(const pugi::xml_node& node, const std::string& relativePath, bool strict)
{
	pugi::xml_node parent = node;
	std::string childName;
	for(auto i = relativePath.begin(); i != relativePath.end(); i++)
	{
		if(*i == '.')
		{
			parent = parent.child(childName.c_str());
			childName.clear();
		}
		else
			childName += *i;
	}
	const auto& ret = childName.size() ? parent.child(childName.c_str()) : parent;
	if(strict && ret.empty())
		throw std::runtime_error(std::string("can't find node <") + relativePath + ">");

	return ret;
}

pugi::xml_node Settings::getNode(const std::string& path, bool strict)
{
	static Settings instance;
	pugi::xml_node parent = instance._doc.first_child();
	return Xml::getNode(parent, path, strict);
}

pugi::xml_attribute Xml::getAttribute(const pugi::xml_node& node, const std::string& name)
{
	const auto& ret = node.attribute(name.c_str());
	if(ret.empty())
		throw std::runtime_error(std::string("can't find attribute <") + name + ">");
	return ret;
}

double Settings::get(const std::string& path, const std::string& name)
{
	return Xml::getAttribute(getNode(path), name).as_double();
}

pugi::xml_attribute Settings::attribute(const std::string& path, const std::string& name)
{
	return Xml::getAttribute(getNode(path), name);
}

double Settings::getRnd(const std::string& path)
{
	return getRnd(getNode(path));
}

std::unique_ptr<RndVariable> RndVariable::make(const pugi::xml_node& node)
{
	std::string distribution(Xml::getAttribute(node, "distribution").as_string());
	const auto& scaledAttr = node.attribute("scaled");
	bool scaled = (!scaledAttr.empty() && scaledAttr.as_bool());

	if(distribution == "constant")
		return std::make_unique<ConstantVariable>(Xml::getAttribute(node, "val").as_double());
	else if(distribution == "uniform")
		return std::make_unique<UniformVariable>(scaled, Xml::getAttribute(node, "min").as_double(), Xml::getAttribute(node, "max").as_double());
	else if(distribution == "normal")
		return std::make_unique<NormalVariable>(scaled, Xml::getAttribute(node, "mean").as_double(), Xml::getAttribute(node, "stddev").as_double());
	else if(distribution == "halfNormal")
		return std::make_unique<HalfNormalVariable>(scaled, Xml::getAttribute(node, "stddev").as_double());
	else if(distribution == "beta")
		return std::make_unique<BetaVariable>(scaled, Xml::getAttribute(node, "alpha").as_double(), Xml::getAttribute(node, "beta").as_double());
	else if(distribution == "pareto")
		return std::make_unique<ParetoVariable>(scaled, Xml::getAttribute(node, "alpha").as_double(), Xml::getAttribute(node, "Xm").as_double());
	throw std::runtime_error("RndVariable::make: unknown distribution");
}

double Settings::getRnd(const pugi::xml_node& node)
{
	auto var = RndVariable::make(node);
	var->init();
	return var->get();
}

bool Settings::exist(const std::string& path, const std::string& name)
{
	const auto& node = getNode(path, false);
	return (!node.empty() && (name.empty() || !node.attribute(name.c_str()).empty()));
}

double linearInterpolation(const std::pair<double, double>& a, const std::pair<double, double>& b, double x)
{
	double t = (std::abs(b.first - a.first) < (FLT_MIN * 100.0)) ? 0.5 :
			(x - a.first) / (b.first - a.first);
	return (t * b.second) + ((1.0 - t) * a.second);
}
