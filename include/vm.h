/*
 * This file is part of the Charly Virtual Machine (https://github.com/KCreate/charly-vm)
 *
 * MIT License
 *
 * Copyright (c) 2017 - 2019 Leonard Schütz
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <iostream>
#include <optional>
#include <queue>
#include <map>
#include <atomic>
#include <chrono>

#include "defines.h"
#include "gc.h"
#include "instructionblock.h"
#include "internals.h"
#include "opcode.h"
#include "status.h"
#include "stringpool.h"
#include "symboltable.h"
#include "value.h"

#include "compiler-manager.h"
#include "compiler.h"
#include "parser.h"
#include "sourcefile.h"

#include "async_task.h"

#pragma once

namespace Charly {

struct VMInstructionProfileEntry {
  uint64_t encountered = 0;
  uint64_t average_length = 0;
};

// Stores how often each type of instruction was encountered
// and how it took on average
class VMInstructionProfile {
public:
  VMInstructionProfile() : entries(nullptr) {
    this->entries = new VMInstructionProfileEntry[kOpcodeCount];
  }
  ~VMInstructionProfile() {
    delete[] this->entries;
  }

  void add_entry(Opcode opcode, uint64_t length) {
    VMInstructionProfileEntry& entry = this->entries[opcode];
    entry.average_length = (entry.average_length * entry.encountered + length) / (entry.encountered + 1);
    entry.encountered += 1;
  }

  VMInstructionProfileEntry* entries;
};

struct VMContext {
  SymbolTable& symtable;
  StringPool& stringpool;
  Compilation::CompilerManager& compiler_manager;

  bool instruction_profile = false;
  bool trace_opcodes = false;
  bool trace_catchtables = false;
  bool trace_frames = false;
  bool trace_gc = false;
  bool verbose_addresses = false;
  bool single_worker_thread = false;

  std::istream& in_stream = std::cin;
  std::ostream& out_stream = std::cout;
  std::ostream& err_stream = std::cerr;
};

/*
 * Stores information about a callback the VM needs to execute
 * */
struct VMTask {
  uint64_t uid;
  VALUE fn;
  VALUE argument;

  VMTask(uint64_t u, VALUE f, VALUE a) : uid(u), fn(f), argument(a) {
  }

  VMTask(VALUE f, VALUE a) : uid(0), fn(f), argument(a) {
  }
};

/*
 * Represents the state of a single worker thread
 * */
struct WorkerThread {
  uint16_t tid;
  bool currently_executing_task;
  std::thread th;

  WorkerThread(std::function<void(void*, uint16_t)> fn, void* vm_handle, uint16_t _tid)
      : tid(_tid), currently_executing_task(false), th(fn, vm_handle, _tid) {
  }

  ~WorkerThread() {
    if (this->th.joinable()) this->th.join();
  }
};

class VM {
  friend GarbageCollector;
  friend ManagedContext;

public:
  VM(VMContext& ctx)
      : context(ctx),
        gc(GarbageCollectorConfig{.out_stream = ctx.err_stream, .err_stream = ctx.err_stream, .trace = ctx.trace_gc}, this),
        running(true),
        frames(nullptr),
        catchstack(nullptr),
        ip(nullptr),
        halted(false) {

    // Determine how many worker threads to spawn
    uint16_t num_threads = std::max(std::thread::hardware_concurrency(), static_cast<uint32_t>(32));
    if (ctx.single_worker_thread) num_threads = 1;

    // Start all the worker threads
    for (int i = 0; i < num_threads; i++) {
      this->worker_threads.push_back(new WorkerThread(this->worker_thread_handler, this, i));
    }

    this->exec_prelude();
  }
  VM(const VM& other) = delete;
  VM(VM&& other) = delete;
  ~VM() {
    this->exit(0);
    this->gc.do_collect();

    // Deallocate worker threads
    auto it = this->worker_threads.begin();
    while (it != this->worker_threads.end()) {
      WorkerThread* wt = *it;
      delete wt;
      this->worker_threads.erase(it);
    }
  }

  // Methods that operate on the VM's frames
  Frame* pop_frame();
  Frame* create_frame(VALUE self, Function* calling_function, uint8_t* return_address, bool halt_after_return = false);
  Frame* create_frame(VALUE self,
                      Frame* parent_environment_frame,
                      uint32_t lvarcount,
                      uint8_t* return_address,
                      bool halt_after_return = false);

  // Stack manipulation
  VALUE pop_stack();
  void push_stack(VALUE value);

  // CatchStack manipulation
  CatchTable* create_catchtable(uint8_t* address);
  CatchTable* pop_catchtable();
  void unwind_catchstack();

  // Methods to create new data types
  VALUE create_object(uint32_t initial_capacity);
  VALUE create_array(uint32_t initial_capacity);
  VALUE create_string(const char* data, uint32_t length);
  VALUE create_string(const std::string& str);
  VALUE create_weak_string(char* data, uint32_t length);
  VALUE create_empty_short_string();
  VALUE create_function(VALUE name,
                        uint8_t* body_address,
                        uint32_t argc,
                        uint32_t lvarcount,
                        bool anonymous,
                        bool needs_arguments);
  VALUE create_cfunction(VALUE name, uint32_t argc, void* pointer);
  VALUE create_generator(VALUE name, uint8_t* resume_address);
  VALUE create_class(VALUE name);
  VALUE create_cpointer(void* data, void* destructor);
  VALUE create_symbol(VALUE value);

  // Methods to copy existing data types
  VALUE copy_value(VALUE value);
  VALUE deep_copy_value(VALUE value);
  VALUE copy_object(VALUE object);
  VALUE deep_copy_object(VALUE object);
  VALUE copy_array(VALUE array);
  VALUE deep_copy_array(VALUE array);
  VALUE copy_string(VALUE string);
  VALUE copy_function(VALUE function);
  VALUE copy_cfunction(VALUE cfunction);
  VALUE copy_generator(VALUE generator);

  // Arithmetics
  VALUE add(VALUE left, VALUE right);
  VALUE sub(VALUE left, VALUE right);
  VALUE mul(VALUE left, VALUE right);
  VALUE div(VALUE left, VALUE right);
  VALUE mod(VALUE left, VALUE right);
  VALUE pow(VALUE left, VALUE right);
  VALUE uadd(VALUE value);
  VALUE usub(VALUE value);

  // Comparison operators
  VALUE eq(VALUE left, VALUE right);
  VALUE neq(VALUE left, VALUE right);
  VALUE lt(VALUE left, VALUE right);
  VALUE gt(VALUE left, VALUE right);
  VALUE le(VALUE left, VALUE right);
  VALUE ge(VALUE left, VALUE right);
  VALUE unot(VALUE value);

  // Bitwise operators
  VALUE shl(VALUE left, VALUE right);
  VALUE shr(VALUE left, VALUE right);
  VALUE band(VALUE left, VALUE right);
  VALUE bor(VALUE left, VALUE right);
  VALUE bxor(VALUE left, VALUE right);
  VALUE ubnot(VALUE value);

  // Machine functionality
  VALUE readmembersymbol(VALUE source, VALUE symbol);
  VALUE setmembersymbol(VALUE target, VALUE symbol, VALUE value);
  VALUE readmembervalue(VALUE source, VALUE value);
  VALUE setmembervalue(VALUE target, VALUE member_value, VALUE value);
  std::optional<VALUE> findprototypevalue(Class* source, VALUE symbol);
  std::optional<VALUE> findprimitivevalue(VALUE value, VALUE symbol);
  VALUE call_dynamic(VALUE v, const std::vector<VALUE>& args, VALUE target = kNull);
  void call(uint32_t argc, bool with_target, bool halt_after_return = false);
  void call_function(Function* function, uint32_t argc, VALUE* argv, VALUE self, bool halt_after_return = false);
  void call_cfunction(CFunction* function, uint32_t argc, VALUE* argv);
  void call_class(Class* klass, uint32_t argc, VALUE* argv);
  void call_generator(Generator* klass, uint32_t argc, VALUE* argv);
  void initialize_member_properties(Class* klass, Object* object);
  bool invoke_class_constructors(Class* klass, Object* object, uint32_t argc, VALUE* argv);
  void throw_exception(const std::string& message);
  void throw_exception(VALUE payload);
  VALUE stacktrace_array();
  void panic(STATUS reason);
  void stacktrace(std::ostream& io);
  void catchstacktrace(std::ostream& io);
  void stackdump(std::ostream& io);
  void inline pretty_print(std::ostream& io, void* value) {
    this->pretty_print(io, (VALUE)value);
  }
  void pretty_print(std::ostream& io, VALUE value);
  void to_s(std::ostream& io, VALUE value, uint32_t depth = 0);

  // Private member access
  inline Frame* get_current_frame() {
    return this->frames;
  }

  // Instructions
  Opcode fetch_instruction();
  void op_readlocal(uint32_t index, uint32_t level);
  void op_readmembersymbol(VALUE symbol);
  void op_readmembervalue();
  void op_readarrayindex(uint32_t index);
  void op_setlocalpush(uint32_t index, uint32_t level);
  void op_setmembersymbolpush(VALUE symbol);
  void op_setmembervaluepush();
  void op_setarrayindexpush(uint32_t index);
  void op_setlocal(uint32_t index, uint32_t level);
  void op_setmembersymbol(VALUE symbol);
  void op_setmembervalue();
  void op_setarrayindex(uint32_t index);
  void op_putself(uint32_t level);
  void op_putvalue(VALUE value);
  void op_putstring(char* data, uint32_t length);
  void op_putfunction(VALUE symbol,
                      uint8_t* body_address,
                      bool anonymous,
                      bool needs_arguments,
                      uint32_t argc,
                      uint32_t lvarcount);
  void op_putcfunction(VALUE symbol, void* pointer, uint32_t argc);
  void op_putgenerator(VALUE symbol, uint8_t* resume_address);
  void op_putarray(uint32_t count);
  void op_puthash(uint32_t count);
  void op_putclass(VALUE name,
                   uint32_t propertycount,
                   uint32_t staticpropertycount,
                   uint32_t methodcount,
                   uint32_t staticmethodcount,
                   bool has_parent_class,
                   bool has_constructor);
  void op_pop();
  void op_dup();
  void op_dupn(uint32_t count);
  void op_swap();
  void op_call(uint32_t argc);
  void op_callmember(uint32_t argc);
  void op_return();
  void op_yield();
  void op_throw();
  void op_registercatchtable(int32_t offset);
  void op_popcatchtable();
  void op_branch(int32_t offset);
  void op_branchif(int32_t offset);
  void op_branchunless(int32_t offset);
  void op_branchlt(int32_t offset);
  void op_branchgt(int32_t offset);
  void op_branchle(int32_t offset);
  void op_branchge(int32_t offset);
  void op_brancheq(int32_t offset);
  void op_branchneq(int32_t offset);
  void op_typeof();

  inline void set_primitive_value(VALUE value) {
    this->primitive_value = value;
  }
  inline void set_primitive_object(VALUE value) {
    this->primitive_object = value;
  }
  inline void set_primitive_class(VALUE value) {
    this->primitive_class = value;
  }
  inline void set_primitive_array(VALUE value) {
    this->primitive_array = value;
  }
  inline void set_primitive_string(VALUE value) {
    this->primitive_string = value;
  }
  inline void set_primitive_number(VALUE value) {
    this->primitive_number = value;
  }
  inline void set_primitive_function(VALUE value) {
    this->primitive_function = value;
  }
  inline void set_primitive_generator(VALUE value) {
    this->primitive_generator = value;
  }
  inline void set_primitive_boolean(VALUE value) {
    this->primitive_boolean = value;
  }
  inline void set_primitive_null(VALUE value) {
    this->primitive_null = value;
  }

  VMContext context;
  VMInstructionProfile instruction_profile;

  inline uint8_t* get_ip() {
    return this->ip;
  }

  void run();
  void exec_prelude();
  VALUE exec_module(Function* fn);
  VALUE exec_function(Function* fn, VALUE argument);
  uint8_t start_runtime();
  void exit(uint8_t status_code);

  VALUE register_module(InstructionBlock* block);
  void register_task(VMTask task);
  uint64_t register_timer(Timestamp, VMTask task);
  uint64_t register_interval(uint32_t, VMTask task);

  uint64_t get_next_timer_id();

  void clear_timer(uint64_t uid);
  void clear_interval(uint64_t uid);

  static void worker_thread_handler(void* vm_handle, uint16_t tid);

  void register_worker_task(AsyncTask task);

  std::chrono::time_point<std::chrono::high_resolution_clock> starttime;
private:

  uint8_t status_code = 0;

  GarbageCollector gc;

  // Used to avoid an overflow when printing cyclic data structures
  std::vector<VALUE> pretty_print_stack;

  // References to the primitive classes of the VM
  VALUE primitive_value = kNull;
  VALUE primitive_object = kNull;
  VALUE primitive_class = kNull;
  VALUE primitive_array = kNull;
  VALUE primitive_string = kNull;
  VALUE primitive_number = kNull;
  VALUE primitive_function = kNull;
  VALUE primitive_generator = kNull;
  VALUE primitive_boolean = kNull;
  VALUE primitive_null = kNull;

  // Contains all tasks that still need to be run
  std::queue<VMTask> task_queue;
  bool running;

  // Remaining timers & intervals
  std::map<Timestamp, VMTask> timers;
  std::map<Timestamp, std::tuple<VMTask, uint32_t>> intervals;

  uint64_t next_timer_id = 0;

  // Worker threads
  bool worker_threads_active = true;
  std::vector<WorkerThread*> worker_threads;

  // Holds the remaining tasks
  std::condition_variable worker_task_queue_cv;
  std::mutex worker_task_queue_m;
  std::queue<AsyncTask> worker_task_queue;

  // Finished tasks which are ready to be handled by the vm
  std::condition_variable worker_result_queue_cv;
  std::mutex worker_result_queue_m;
  std::queue<AsyncTaskResult> worker_result_queue;

  // Holds a pointer to the upper-most environment frame
  // When executing new modules, their parent environment frame is set to
  // this frame, so they are not able to interact with the calling module
  //
  // Both modules can still communicate with each other via the several global
  // objects & methods.
  Frame* top_frame;

  // Holds the last value that was thrown as an exception
  VALUE last_exception_thrown;

  std::vector<VALUE> stack;
  Frame* frames;
  CatchTable* catchstack;
  uint8_t* ip;
  bool halted;
};
}  // namespace Charly
