#ifndef TKG_CONFIG_HPP
#define TKG_CONFIG_HPP

#include <string>
#include <map>
#include <vector>
#include <fstream>

#include <iostream>

namespace tkg
{

class ConfigValue : public std::vector<std::string>
{
    public:

        operator std::string() const
        {
            return empty() ? std::string() : front();
        }

        bool parse(const std::string &str)
        {
            for(size_type p=0,n=0; true; p=n+1)
            {
                n = str.find_first_of(",;", p);
                if(n == std::string::npos) break;

                push_back( str.substr(p,n-p) );
            }
            return true;
        }

        void print(std::string key)
        {
            std::cout << key << " = ";
            for(int i=0; i<size(); i++)
            {
                std::cout << at(i) << " ";
            }
            std::cout << std::endl;
        }
};

class ConfigGroup : public std::map<std::string, ConfigValue>
{
    public:

        bool parse(const std::string &str)
        {
            for(size_type p=0,s=0,g=0; true; p=g+1)
            {
                s = str.find_first_of(':', p);
                g = str.find_first_of(';', p);

                if(s == std::string::npos) break;
                if(g == std::string::npos) break;

                ConfigValue val;
                if( !val.parse(str.substr(s+1, g-s)) ) return false;

                insert( make_pair(str.substr(p,s-p), val) );
            }
            return true;
        }

        void print(std::string key)
        {
            for(std::map<std::string, ConfigValue>::iterator it=begin(); it!=end(); it++)
            {
                it->second.print(key + std::string("[") + it->first + std::string("]"));
            }
        }
};

class ConfigFile : public std::map<std::string, ConfigGroup>
{
    public:

        bool load(const char* file)
        {
            std::ifstream fin(file);
            if(!fin) return false;

            std::string str,buf;
            while(fin >> buf)
            {
                str += buf;
            }
            return parse(str);
        }

        bool parse(const std::string &str)
        {
            for(size_type p=0,s=0,g=0; true; p=g+1)
            {
                s = str.find_first_of('{', p);
                g = str.find_first_of('}', p);

                if(s == std::string::npos) break;
                if(g == std::string::npos) break;

                ConfigGroup val;
                if( !val.parse(str.substr(s+1, g-s-1)) ) return false;

                insert( make_pair(str.substr(p,s-p), val) );
            }
            return true;
        }

        void print()
        {
            for(std::map<std::string, ConfigGroup>::iterator it=begin(); it!=end(); it++)
            {
                it->second.print(std::string("[") + it->first + std::string("]"));
            }
        }
};

}

#endif // TKG_CONFIG_HPP

