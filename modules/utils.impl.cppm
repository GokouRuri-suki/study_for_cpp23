export module utils:impl;
import std;

export void list_print(auto& list){
  for (auto &l : list)
    std::print("{} ", l);
}