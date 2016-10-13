#include <iostream>
#include <fstream>
#include <cassert>
#include <string>
#include <map>

int main()
{
	long checked=0,created=0;
	std::ifstream f("pkt_mem_leak.txt");
	assert(f!=NULL);
	std::map<std::string,std::string> map;
	std::string addr,flag;
	while(!f.eof())
	{
		f>>addr>>flag;
		if(!flag.compare("created"))
		{
			map[addr]=flag;
			created++;

		}
		if(!flag.compare("deleted"))
		{
			if(map.end() != map.find(addr))
			{
				map.erase(addr);
				//std::cout<<"checked packet "<<addr<<std::endl;
				checked++;
			}

			else
				assert(1!=0);
		}
		
		//std::cout<<addr<<" "<<flag<<std::endl;
	}
	
//	for(auto it=map.begin() ;it !=map.end();it++)
//		std::cout<<"not deleted "<<it->first<<std::endl;

	std::cout<<"created = "<<created<<" checked = "<<checked<<" !! LOST(unchecked) = "<<created-checked<<std::endl;
	return 0;
}
