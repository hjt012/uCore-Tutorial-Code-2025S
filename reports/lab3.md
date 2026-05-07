# Lab3 实验报告

**姓名**：胡峻滔
**班级**：计科2402
**学号**：2024040016

---

## 一、功能实现

本次实验实现了以下功能：

1. **spawn 系统调用**：相当于 fork + exec 的组合，创建新子进程并直接加载指定程序执行。实现中调用 allocproc 分配 PCB，设置父子关系，复用 loader 加载程序，将子进程 trapframe->a0 设为 0 表示子进程返回值。

2. **stride 调度算法**：为每个进程维护 stride（当前累计运行量）和 pass（步长，由优先级决定）。调度器遍历所有 RUNNABLE 进程，选择 stride 最小的进程运行，运行后将其 stride += pass。优先级越高，pass 越小，获得的 CPU 时间越多。

3. **set_priority 系统调用**：允许进程设置自身优先级（>=2），内核同步更新 pass = BIG_STRIDE / priority。

测试通过 make test 验证，Test many spawn OK! 和 ch5t usertest passed! 确认功能正确。

---

## 二、问答题

### Q1: stride 算法溢出问题

p1.stride = 255, p2.stride = 250, pass = 10，使用 8bit 无符号整型。

p2 执行后 stride 变为 260，但 8bit 只能表示 0-255，260 溢出后变为 4。此时 p1.stride = 255, p2.stride = 4，所以 p2 会再次被调度，而不是 p1。这就是溢出导致的不公平。

---

### Q2: STRIDE_MAX - STRIDE_MIN <= BIG_STRIDE / 2

在优先级全部 >= 2 的情况下（即 pass <= BIG_STRIDE/2），每次调度 stride 最小的进程，其 stride 增加 pass。由于 pass <= BIG_STRIDE/2，任意两个进程的 stride 差值不会超过 BIG_STRIDE/2。否则若差值超过一半，说明小的进程很久没被调度，违反了 stride 算法的公平性。

---

### Q3: 比较函数实现

```c
typedef unsigned long long Stride_t;  
const Stride_t BIG_STRIDE = 0xffffffffffffffffULL;

int cmp(Stride_t a, Stride_t b) {  
if (a == b) return 0;  
Stride_t diff = b - a;  
if (diff <= BIG_STRIDE / 2)  
return -1; // a < b  
else  
return 1; // a > b（发生绕回）  
}
```


验证例子（8bit, BIG_STRIDE=255）：
- cmp(125, 255)：diff = 130 > 127，绕回 → 返回 1 ✓
- cmp(129, 255)：diff = 126 <= 127 → 返回 -1 ✓

---

## 三、荣誉准则

在完成本次实验的过程（含此前学习的过程）中，我曾分别与以下各位就（与本次实验相关的）以下方面做过交流，还在代码中对应的位置以注释形式记录了具体的交流对象及内容：

**无**

此外，我也参考了以下资料，还在代码中对应的位置以注释形式记录了具体的参考来源及内容：

1. uCore-Tutorial-Guide 2025 Spring 官方文档

我独立完成了本次实验除以上方面之外的所有工作，包括代码与文档。我清楚地知道，从以上方面获得的信息在一定程度上降低了实验难度，可能会影响起评分。

我从未使用过他人的代码，不管是原封不动地复制，还是经过了某些等价转换。我未曾也不会向他人（含此后各届同学）复制或公开我的实验代码，我有义务妥善保管好它们。我提交至本实验的评测系统的代码，均无意于破坏或妨碍任何计算机系统的正常运转。我清楚地知道，以上情况均为本课程纪律所禁止，若违反，对应的实验成绩将按"-100"分计。