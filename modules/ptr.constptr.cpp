module ptr;
import std;
namespace constptr {
void how_to_use_constptr() {
  /**
   * @note const对ptr在c语境下的机制
   *详见书93页
   */
  /*
   *依次为普通指针，指针常量，常量指针
   *指针常量:指向不可变
   *常量指针:值不可变 */
  int *p{nullptr};
  int *const ptr1{nullptr};
  const int *ptr2{nullptr}; // 简单说const右边是指针就是指针不变有
  // *解引用就是值不变
  int *const pp = new int(12);
  int const *pp2 = new int(10);
  *pp = 2;
  pp2 = new int(1);

  // 那都不可变呢
  // 分析：const无视那么第一个const右边是*ptr4第二个是ptr
  // 自然是都不可变
  int const *const ptr4{nullptr};
  delete pp;
  delete pp2;

  return;
}
void how_to_use_constptr_of_ref() {
  /**
   *@note顺便写一下ref
   *ref默认是常量指针不可更改引用的那种
   *
   */
  int x{10};
  int y{5};
  int &xRef{x};
  int &yRef{y};
  xRef = yRef;
  std::print("xRef = {},yRef = {}", xRef, yRef);
};
}; // namespace constptr
