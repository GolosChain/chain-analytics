#ifndef DATAREPRESENTATION_H_
#define DATAREPRESENTATION_H_
#include <string>
#include <map>
#include <vector>
#include "gnuplot-iostream.h"
//-lboost_iostreams -lboost_system -lboost_filesystem

class ProjectedDataRepresentation
{
public:
	using ProjectionStruct = std::pair<std::string, std::vector<std::string> >;
	using PointStruct = std::vector<ProjectionStruct>;
protected:
	struct Projection
	{
		std::string _name;
		std::vector<std::string> _coords;
		std::vector<std::vector<double> > _data;
		Projection(size_t pointsNum, const std::string& name, const std::vector<std::string>& coords);
	};
	std::map<std::string, size_t> _nameToProj;
	std::vector<Projection> _projs;
	size_t _pointsNum;
	bool _colored;
	PointStruct _pointStruct;

	size_t _maxRegisteredPoint;
	size_t _curPoint;
	void resize();
public:
	ProjectedDataRepresentation():
		_pointsNum(0), _colored(false),  _maxRegisteredPoint(0), _curPoint(0) {};

	void init(const PointStruct& pointStruct, bool colored){_pointStruct = pointStruct; _colored = colored;};
	void startSending(){_curPoint = 0;};
	void next(){++_curPoint;};
	void set(const std::string& projName,
			const std::vector<double>&& data);//last value in data is reserved for group id
	virtual void show() = 0;
	virtual ~ProjectedDataRepresentation(){};
	size_t getPointsNum()const {return _pointsNum;};

};

class MeanToConsole : public ProjectedDataRepresentation
{
public:
	void show() override;
};

class GnuplotDisplay final: public ProjectedDataRepresentation
{
	static constexpr std::array<const char*, 2> DIM_NAMES = {"x", "y"};
	Gnuplot _gnuplot;
	std::vector<std::vector<std::pair<double, double> > > _ranges;

	double _zoomInBorder;
	double _zoomOutFactor;
	size_t _pointType;
	const bool _multiplotMode;
	std::string _name;
	size_t _rowSize;
	void updateRanges();
	void showMultiplot();
	void showSingleplot();
	std::string getPointsStr() const;
public:
	GnuplotDisplay(const std::string& name, double zoomInBorder, double zoomOutFactor, size_t pointType, bool multiplotMode = true, size_t rowSize = 0):
		_zoomInBorder(zoomInBorder), _zoomOutFactor(zoomOutFactor), _pointType(pointType), _multiplotMode(multiplotMode),
		_name(name), _rowSize(rowSize){};
	void show() override;
};

#endif /* DATAREPRESENTATION_H_ */
