#include <iostream>
#include "DataRepresentation.h"

Gnuplot _gnuplot;
ProjectedDataRepresentation::Projection::Projection(size_t pointsNum, const std::string& name, const std::vector<std::string>& coords)
: _name(name), _coords(coords), _data(pointsNum, std::vector<double>(_coords.size() + 1, 0.0)){}

void ProjectedDataRepresentation::resize()
{
	_pointsNum = _maxRegisteredPoint + 1;
	_projs.clear();
	size_t p = 0;
	_projs.reserve(_pointStruct.size());
	for(const auto& projStruct : _pointStruct)
	{
		_projs.emplace_back(Projection(_pointsNum, projStruct.first, projStruct.second));
		_nameToProj[projStruct.first] = p++;
	}
}

void ProjectedDataRepresentation::set(const std::string& projName, const std::vector<double>&& data)
{
	_maxRegisteredPoint = std::max(_maxRegisteredPoint, _curPoint);
	if((_curPoint == 0) && (_maxRegisteredPoint >= _pointsNum))
		resize();
	if(_curPoint < _pointsNum)
	{
		auto iProj = _nameToProj.find(projName);
		if(iProj == _nameToProj.end())
			throw std::logic_error("ProjectedDataRepresentation::set: wrong projection's name");
		auto& proj = _projs.at(iProj->second);
		size_t size = proj._coords.size();
		if(_colored)
			++size;
		if(size != data.size())
			throw std::logic_error("ProjectedDataRepresentation::set: wrong data format");

		proj._data.at(_curPoint) = std::move(data);
	}
}

void MeanToConsole::show()
{
	for(auto& proj : _projs)
	{
		size_t coordsNum = proj._coords.size();
		std::cout << proj._name << "\n";
		for(size_t crd = 0; crd < coordsNum; crd++)
		{
			double mean = 0.0;
			for(auto& point : proj._data)
				mean += point.at(crd);
			mean /= static_cast<double>(proj._data.size());
			std::cout << "  " << proj._coords[crd] << " = " << mean << " \n";
		}
	}
}

void GnuplotDisplay::updateRanges()
{
	size_t p = 0;
	_ranges.resize(_projs.size());
	for(auto& proj : _projs)
	{
		auto& curRanges = _ranges[p];
		size_t coordsNum = proj._coords.size();
		std::vector<double> mins(coordsNum, std::numeric_limits<double>::max());
		std::vector<double> maxs(coordsNum, -std::numeric_limits<double>::max());
		for(auto& point : proj._data)
		{
			for(size_t crd = 0; crd < coordsNum; crd++)
			{
				double curVal = point.at(crd);
				mins[crd] = std::min(mins[crd], curVal);
				maxs[crd] = std::max(maxs[crd], curVal);
			}
		}
		curRanges.resize(coordsNum, std::make_pair(0.0, 0.0));
		bool reset = false;
		for(size_t crd = 0; (crd < coordsNum) && !reset; crd++)
		{
			auto& range = curRanges[crd];
			double rangeL = range.second - range.first;
			double valsL = maxs[crd] - mins[crd];
			if((rangeL == 0.0) ||
					(range.first  > mins[crd]) ||
					(range.second < maxs[crd]) || ((valsL / rangeL) < _zoomInBorder))
				reset = true;
		}
		if(reset)
			for(size_t crd = 0; crd < coordsNum; crd++)
			{
				auto& range = curRanges[crd];
				double mean = (mins[crd] + maxs[crd]) * 0.5;
				double dist = (maxs[crd] - mins[crd]) * 0.5 * _zoomOutFactor;
				dist = std::max(dist, 0.0001);
				range.first =  mean - dist;
				range.second = mean + dist;
			}
		p++;
	}
}

constexpr std::array<const char*, 2> GnuplotDisplay::DIM_NAMES;

std::string GnuplotDisplay::getPointsStr() const
{
	return std::string((_pointType ? ("with points pointtype " + std::to_string(_pointType) ) :
						"with lines ")) + (_colored ? " lt palette " : "");
}

void GnuplotDisplay::showMultiplot()
{
	size_t plotsNum = _projs.size();

	size_t cols = _rowSize ? _rowSize : static_cast<size_t>(std::ceil(sqrt(static_cast<double>(plotsNum))));
	size_t rows = static_cast<size_t>(std::ceil(static_cast<double>(_projs.size()) / static_cast<double>(cols)));

	_gnuplot << "set multiplot layout " <<
			std::to_string(rows) << "," <<
			std::to_string(cols) <<  " rowsfirst" << std::endl;

	size_t p = 0;

	std::string pointsStr = getPointsStr();

	for(auto& proj : _projs)
	{
		_gnuplot << "unset key" << std::endl;
		auto& ranges = _ranges.at(p);
		size_t dim = ranges.size();
		if(dim > 3)
			throw std::runtime_error("GnuplotDisplay::show: unsupported dimension");

		if(dim <= DIM_NAMES.size())
		{
			_gnuplot << "unset colorbox" << std::endl;
			size_t d = 0;
			for(; d < dim; d++)
			{
				_gnuplot << "set " << DIM_NAMES[d] << "range [" <<
							std::to_string(ranges.at(d).first)  << ":" <<
							std::to_string(ranges.at(d).second) << "]" << std::endl;
				_gnuplot << "set " << DIM_NAMES[d] << "label '"
							<< proj._name + (proj._coords[d].empty() ? "" : ("." +  proj._coords[d]))
							<< "'" << std::endl;
				_gnuplot << "set " << DIM_NAMES[d] << "tics auto" << std::endl;
			}
			for(;d < DIM_NAMES.size(); d++)
			{
				_gnuplot << "set " << DIM_NAMES[d] << "range [-1:1]" << std::endl;
				_gnuplot << "unset " << DIM_NAMES[d] << "label" << std::endl;
				_gnuplot << "unset " << DIM_NAMES[d] << "tics" << std::endl;
			}
		}

		if(dim == 1)
			_gnuplot << "plot" << _gnuplot.file1d(proj._data) << " using 1:(0):2 "
			<< pointsStr << std::endl;
		else if(dim == 2)
			_gnuplot << "plot" << _gnuplot.file1d(proj._data) <<
			pointsStr << std::endl;
		else if(dim == 3)//это heatmap, здесь отрисовка отличается
		{
			_gnuplot << "set colorbox" << std::endl;
			_gnuplot << "set autoscale fix" << std::endl;
			_gnuplot << "set format x ''" << std::endl;
			_gnuplot << "set format y ''" << std::endl;
			_gnuplot << "plot " <<  _gnuplot.file1d(proj._data) << " using 1:2:3 with image" << std::endl;
		}

		else
			throw std::runtime_error("GnuplotDisplay::show: unsupported dimension");
		p++;
	}
}

void GnuplotDisplay::showSingleplot()
{
	std::vector<std::pair<double, double> > looseRange(DIM_NAMES.size(), std::make_pair(
					 std::numeric_limits<double>::max(),
					-std::numeric_limits<double>::max()));
	size_t commonDim = 0;
	for(size_t p = 0; p < _projs.size(); p++)
	{
		auto& ranges = _ranges.at(p);
		size_t dim = ranges.size();
		if(commonDim && (dim != commonDim))
			throw std::runtime_error("GnuplotDisplay::show: singleplot mode, unsupported dimension");
		if(dim > DIM_NAMES.size())
			throw std::runtime_error("GnuplotDisplay::show: unsupported dimension");
		for(size_t d = 0; d < dim; d++)
		{
			looseRange[d].first  = std::min(looseRange[d].first,  ranges.at(d).first);
			looseRange[d].second = std::max(looseRange[d].second, ranges.at(d).second);
		}
		commonDim = dim;
	}
	if(!commonDim)
		throw std::logic_error("GnuplotDisplay::show: common dimension is equal to zero");
	for(size_t d = 0; d < looseRange.size(); d++)
		_gnuplot << "set " << DIM_NAMES[d] << "range [" <<
				std::to_string(looseRange[d].first)  << ":" <<
				std::to_string(looseRange[d].second) << "]" << std::endl;

	size_t p = 0;
	_gnuplot << "plot";
	std::string pointsStr = getPointsStr();

	for(auto& proj : _projs)
	{
		if(p)
			_gnuplot << ", ";
		if(commonDim == 1)
			_gnuplot << _gnuplot.file1d(proj._data) << " using 1:(0) ";
		else if(commonDim == 2)
			_gnuplot << _gnuplot.file1d(proj._data);
		else
			throw std::runtime_error("GnuplotDisplay::show: unsupported dimension");
			_gnuplot << pointsStr;
		_gnuplot << " title '" << proj._name << ": ";
		for(size_t d = 0; d < commonDim; d++)
			_gnuplot << proj._coords[d] << " / ";
		_gnuplot << "'";
		p++;
	}
	_gnuplot << std::endl;
}

void GnuplotDisplay::show()
{
	updateRanges();
	_gnuplot << "unset multiplot" << std::endl;
	//_gnuplot << "set term wxt title '" << _name << "'" << std::endl;
	if(_multiplotMode)
		showMultiplot();
	else
		showSingleplot();
}
