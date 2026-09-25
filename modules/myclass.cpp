module myclass;
import std;

void dog::bark() { std::println("汪汪汪！"); };
void dog::get_info() {
  std::println("名字:{}\n性别:{}\n年龄:{}", _name, _sex, _age);
};

dog::dog(std::string name, std::string sex, int age)

    : _name{name}, _sex{sex}, _age{age} {}
dog::dog() : _name{"dog"}, _sex{"M"}, _age{0} {}
dog::~dog() { std::printf("dog 析构执行"); }
