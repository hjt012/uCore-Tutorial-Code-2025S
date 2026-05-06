# Lab1 实验报告

**姓名**：胡峻滔
班级：计科2402
**学号**：2024040016

---

## 一、功能实现（200字以内）

本次实验实现了 `sys_trace` 系统调用（syscall ID: 410），支持三种功能：

1. **读取字节**（trace_request=0）：读取当前任务指定地址处的一个字节
2. **写入字节**（trace_request=1）：向当前任务指定地址写入一个字节
3. **查询系统调用次数**（trace_request=2）：查询当前任务某个系统调用的累计调用次数

实现方式为：在进程控制块（PCB）中添加 `syscall_counts` 数组记录每个系统调用的次数；在 `syscall()` 入口处对每次系统调用进行计数；在 `sys_trace` 函数中根据请求类型执行对应操作。测试通过 `make test` 验证，`Test trace OK!` 确认功能正确。

---

## 二、问答题

### Q1: 使用 S 态特权指令或访问 S 态寄存器的行为

在用户态（U-mode）执行 S 态特权指令（如 `sret`、`wfi`、`sfence.vma`）或访问 S 态 CSR 寄存器（如 `sstatus`、`sepc`、`stvec` 等）会触发 **Illegal Instruction（非法指令）异常**。

**测试说明**：参考测例 `__ch2_bad_instruction`、`__ch2_bad_register`，用户程序尝试执行 `sret` 指令或访问 S 态寄存器，会触发异常并被内核捕获，内核打印错误信息后终止该程序。

**使用的 SBI 及版本**：RustSBI-QEMU Version 0.2.0-alpha.2（RustSBI version 0.3.0-alpha.2，adapting to RISC-V SBI v1.0.0）。

---

### Q2: trampoline.S 中 userret 和 uservec 的作用

**uservec**：用户态 Trap 入口，由 `stvec` 指向。负责将用户态寄存器保存到 `trapframe` 中，然后跳转到 C 函数 `usertrap` 处理。

**userret**：用户态 Trap 返回出口。负责从 `trapframe` 恢复用户态寄存器，然后执行 `sret` 返回用户态。

---

### Q3: L79 - 刚进入 userret 时，a0、a1 分别代表什么值？

`a0`：指向当前进程 `trapframe` 结构体的指针（由 `usertrapret` 作为第一个参数传入）。

`a1`：用户页表的 satp 值（由 `usertrapret` 作为第二个参数传入），当前章节中此值未实际使用。

---

### Q4: L87-L88 - sfence 指令有何作用？删掉会导致错误吗？


`sfence.vma` 是**刷新 TLB（页表缓存）**的指令。当 `satp` 寄存器被修改（切换页表）后，必须执行 `sfence.vma` 确保旧的地址映射缓存被清除，新页表生效。

**当前章节**：因为没有启用虚存机制，删除这两行指令不会导致错误。但在后续章节启用页表后，删掉它会导致地址翻译错误。

---

### Q5: L96-L125 - 为何注释中说要除去 a0？a0 的值存在何处？


**为何除去 a0**：用户态的 `a0` 值已经在最开始时通过 `csrrw a0, sscratch, a0` 交换到了 `sscratch` 中，并且 `trapframe` 中偏移 112 处保存了用户 `a0` 的副本。

**a0 现在存在何处**：用户态 `a0` 的备份保存在：
- `trapframe` 偏移 112 处（由 uservec 中 `sd t0, 112(a0)` 保存）
- `sscratch` 中保存了 `trapframe` 的地址，最后通过 `csrrw a0, sscratch, a0` 交换恢复

---

### Q6: userret 中发生状态切换在哪一条指令？为何执行之后会进入用户态？

状态切换发生在 **`sret`** 指令。

执行 `sret` 后：
1. CPU 将 `sstatus.SPP` 的值恢复到当前特权级（用户态 U-mode）
2. PC 跳转到 `sepc` 寄存器保存的地址（用户程序被中断的位置）
3. `sstatus.SIE` 恢复到 `sstatus.SPIE` 的值

因此 `sret` 执行后即从 S 态切换回 U 态。

---

### Q7: L29 - csrrw a0, sscratch, a0 执行后，a0 和 sscratch 中各是什么值？


这是**csr 原子交换**指令：
- 执行前：`a0` = 用户态传入的值，`sscratch` = `trapframe` 地址
- 执行后：`a0` = `trapframe` 地址（原 `sscratch` 的值），`sscratch` = 用户态原 `a0` 的值

这样用户态 `a0` 被保存在 `sscratch` 中，而 `a0` 指向 `trapframe`，用于后续保存其他寄存器。

---

### Q8: L32-L61 - 从 trapframe 第几项开始保存？为什么？

从**偏移 40 处**（`ra` 寄存器）开始保存，对应 `trapframe` 结构体中第 6 个字段。

前 40 字节（5 个字段）是内核维护的信息，不需要在 uservec 中保存。**不是所有值都保存**，调用者保存寄存器（如 `t0-t6`、`a0-a7`）由编译器自动保存/恢复，`uservec` 主要保存被调用者保存寄存器（`ra`、`sp`、`s0-s11`）及部分其他寄存器。

---

### Q9: 进入 S 态是哪一条指令发生的？

进入 S 态是通过 **`ecall`** 指令发生的。

当用户态程序执行 `ecall` 指令时，CPU 触发 Environment Call from U-mode 异常，硬件自动：
1. 将当前 PC 保存到 `sepc`
2. 将异常原因（8）写入 `scause`
3. 切换到 S 模式
4. 跳转到 `stvec` 指向的 `uservec` 函数

---

### Q10: L75-L76 - ld t0, 16(a0) 执行后，t0 中的值是什么？

`16(a0)` 对应 `trapframe` 结构中偏移 16 处的 `kernel_trap` 字段。

执行后 **`t0` = `usertrap` 函数地址**。

**由来**：在 `allocproc()` 初始化进程后，`trapframe->kernel_trap` 被设置为 `(uint64)usertrap`（通过 `usertrapret` 函数写入），确保 Trap 进入内核后，能跳转到 C 语言的异常处理函数 `usertrap`。

---

## 三、荣誉准则

在完成本次实验的过程（含此前学习的过程）中，我曾分别与以下各位就（与本次实验相关的）以下方面做过交流，还在代码中对应的位置以注释形式记录了具体的交流对象及内容：

**无**

此外，我也参考了以下资料，还在代码中对应的位置以注释形式记录了具体的参考来源及内容：

1. uCore-Tutorial-Guide 2025 Spring 官方文档

我独立完成了本次实验除以上方面之外的所有工作，包括代码与文档。我清楚地知道，从以上方面获得的信息在一定程度上降低了实验难度，可能会影响起评分。

我从未使用过他人的代码，不管是原封不动地复制，还是经过了某些等价转换。我未曾也不会向他人（含此后各届同学）复制或公开我的实验代码，我有义务妥善保管好它们。我提交至本实验的评测系统的代码，均无意于破坏或妨碍任何计算机系统的正常运转。我清楚地知道，以上情况均为本课程纪律所禁止，若违反，对应的实验成绩将按"-100"分计。
