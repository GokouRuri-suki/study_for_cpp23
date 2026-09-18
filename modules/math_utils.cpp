module math_utils;
import std;
auto list_sum(std::initializer_list<int> list){
  int ret{0};
  for(auto &l : list){
    ret += l;
  }
  return ret;
}