#include <iostream>
#include "GolosEconomy.h"
#include "Utils.h"

#include <iomanip>
#include <iostream>
#include <vector>
#include <future>
#include <nlopt.hpp>

#include <functional>
#include <boost/asio/thread_pool.hpp>
#include <boost/asio.hpp>
#include <boost/filesystem.hpp>


int main(int argc, char* argv[])
{
	std::string inputEnvironmentsFolder((argc > 1) ? argv[1] : "");

	std::list<Environment> environments;

	boost::asio::thread_pool pool(Settings::attribute("main", "threads").as_uint());
	size_t i = 0;
	std::string rulePath(std::string("rules._") + std::to_string(i));
	while((i < Settings::attribute("main", "rulesLimit").as_uint()) && Settings::exist(rulePath))
	{
		for(size_t j = 0; j < Settings::attribute("main", "copies").as_uint(); j++)
		{
			std::string numStr = std::to_string(i) + "_" + std::to_string(j);
			std::string inputFileName;
			if(!inputEnvironmentsFolder.empty())
				inputFileName = inputEnvironmentsFolder + "/_" + numStr + ".xml";
			environments.emplace_back(Environment(std::string("environments_output/_") + numStr + ".xml", inputFileName));
			boost::asio::post(pool, std::bind(&Environment::run, &(environments.back()), rulePath));
		}
		rulePath = std::string("rules._") + std::to_string(++i);
	}
	pool.join();
	std::cout << "done." << std::endl;
	return 0;
}
