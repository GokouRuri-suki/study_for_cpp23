module myclass;
import std;

void animal::bark() { std::println("汪汪汪！"); };
void animal::get_info() {
  std::println("名字:{}\n性别:{}\n年龄:{}", _name, _sex, _age);
};

animal::animal(std::string name, std::string sex, int age)

    : _name{name}, _sex{sex}, _age{age} {}
animal::animal() : _name{"animal"}, _sex{"M"}, _age{0} {}
void animal::who() { std::println("animal"); }
animal::~animal() { std::printf("animal 析构执行"); }

dog::dog(std::string name, std::string sex, int age) {
  animal(name, sex, age);
};
dog::~dog() {};
void dog::who() { std::println("dog"); };

cat::cat(std::string name, std::string sex, int age) {
  animal(name, sex, age);
};
cat::~cat() {};
void cat::who() { std::println("cat"); };
