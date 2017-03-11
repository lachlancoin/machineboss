#ifndef JSONIO_INCLUDED
#define JSONIO_INCLUDED

#include <json.hpp>
#include <fstream>
#include <iostream>
#include "util.h"

template<class Base>
struct JsonLoader : Base {
  void readJson (std::istream& in) {
    nlohmann::json j;
    in >> j;
    Base::readJson(j);
  }
  
  static JsonLoader<Base> fromJson (const nlohmann::json& json) {
    JsonLoader<Base> obj;
    ((Base&)obj).readJson (json);
    return obj;
  }
  
  static JsonLoader<Base> fromJson (std::istream& in) {
    JsonLoader<Base> obj;
    obj.readJson (in);
    return obj;
  }
  
  static JsonLoader<Base> fromFile (const char* filename) {
    std::ifstream infile (filename);
    if (!infile)
      Fail ("File not found: %s", filename);
    return fromJson (infile);
  }
  
  void toFile (const char* filename) {
    std::ofstream outfile (filename);
    if (!outfile)
      Fail ("Couldn't open file: %s", filename);
    Base::writeJson (outfile);
  }

  std::string toJsonString() const {
    std::ostringstream out;
    Base::writeJson (out);
    return out.str();
  }
};

#endif /* JSONIO_INCLUDED */