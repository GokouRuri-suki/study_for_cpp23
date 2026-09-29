import std;
import mystl;
import myclass;
import ptr;
import test;
int main() {
  std::println("test");
  std::vector<int> vec{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  auto v = vec | std::ranges::views::take(3);
  for (auto &x : vec)
    std::print("{}  ", x);
  return 0;
}
