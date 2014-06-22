/*
 *  Multi2Sim
 *  Copyright (C) 2014  Sida Gu (gu.sid@husky.neu.edu)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef ARCH_MIPS_EMU_CONTEXT_H
#define ARCH_MIPS_EMU_CONTEXT_H

#include <iostream>
#include <memory>
#include <vector>

#include <arch/mips/asm/Inst.h>
#include <lib/cpp/ELFReader.h>
#include <memory/Memory.h>
#include <memory/SpecMem.h>
#include <arch/common/FileTable.h>

#include "Regs.h"
#include "Signal.h"


namespace MIPS
{

class Context;
class Emu;


/// Context states
enum ContextState
{
	ContextInvalid      = 0x00000,
	ContextRunning      = 0x00001,  // it is able to run instructions
	ContextSpecMode     = 0x00002,  // executing in speculative mode
	ContextSuspended    = 0x00004,  // suspended in a system call
	ContextHandler      = 0x00040,  // executing a signal handler
	ContextCallback     = 0x10000,  // suspended after syscall with callback
};

/// Context list identifiers
enum ContextListType
{
	// No 'Invalid' identifier here
	ContextListRunning = 0,
	ContextListSuspended,
	ContextListZombie,
	ContextListFinished,

	// Number of context lists
	ContextListCount
};

/// MIPS Context
class Context
{
	// Emulator it belongs to
	Emu *emu;

	// Process ID
	int pid;

	// Virtual memory address space index
	int address_space_index;

	// Context state, expressed as a bitmap of flags, e.g.,
	// ContextSuspended | ContextFutex
	unsigned state;

	// Context memory. This object can be shared by multiple contexts, so it
	// is declared as a shared pointer. The last freed context pointing to
	// this memory object will be the one automatically freeing it.
	std::shared_ptr<mem::Memory> memory;

	// Speculative memory. Its initialization is deferred to be able to link
	// it with the actual memory, known only at context creation.
	std::unique_ptr<mem::SpecMem> spec_mem;

	// Register file. Each context has its own copy always.
	Regs regs;

	// last emulated instruction
	std::unique_ptr<Inst> inst;

	// File descriptor table, shared by contexts
	std::shared_ptr<comm::FileTable> file_table;

	// Instruction pointers
	unsigned last_eip;  // Address of last emulated instruction
	unsigned current_eip;  // Address of currently emulated instruction
	unsigned target_eip;  // Target address for branch, even if not taken

	// Virtual address of the memory access performed by the last emulated
	// instruction.
	unsigned effective_address;

	// Update the context state, updating also the presence on the context
	// in the various context lists in the emulator.
	void UpdateState(unsigned state);

	// Callbacks for suspended contexts
	typedef bool (Context::*CanWakeupFn)();
	typedef void (Context::*WakeupFn)();

	// Stored callbacks for functions used to the wakeup mechanism of
	// suspended contexts. Variable 'wakeup_state' contains the state
	// or states that will be set when suspended and cleared when
	// waken up
	CanWakeupFn can_wakeup_fn;
	WakeupFn wakeup_fn;
	ContextState wakeup_state;

	///////////////////////////////////////////////////////////////////////
	//
	// Functions implemented in ContextLoader.cc. These are the functions
	// related with the program loading process.
	//
	///////////////////////////////////////////////////////////////////////

	/// Structure containing information initialized by the program loader,
	/// associated with a context. When a context is created from a program
	/// executable, a Loader object is associated to it. All child contexts
	/// spawned by it will share the same Loader object.
	struct Loader
	{
		// Program executable
		std::unique_ptr<ELFReader::File> binary;

		// Command-line arguments
		std::vector<std::string> args;

		// Environment variables
		std::vector<std::string> env;

		// Executable interpreter
		std::string interp;

		// Executable file name
		std::string exe;

		// Current working directory
		std::string cwd;

		// File name for standard input and output
		std::string stdin_file_name;
		std::string stdout_file_name;

		// Stack
		unsigned stack_base;
		unsigned stack_top;
		unsigned stack_size;
		unsigned environ_base;

		// Lowest address initialized
		unsigned bottom;

		// Program entries
		unsigned prog_entry;
		unsigned interp_prog_entry;

		// Program headers
		unsigned phdt_base;
		unsigned phdr_count;

		// Random bytes
		unsigned at_random_addr;
		unsigned at_random_addr_holder;

	};

	// String map from program header types
	static misc::StringMap program_header_type_map;

	// Loader information. This information can be shared among multiple
	// contexts. For this reason, it is declared as a shared pointer. The
	// last destructed context sharing this variable will automatically free
	// it.
	std::shared_ptr<Loader> loader;

	// Load ELF sections from binary
	void LoadELFSections(ELFReader::File *binary);

	// Load program headers
	void LoadProgramHeaders();

	// Load ELF binary, as already decoded in 'loader.binary'
	void LoadBinary();

	// Load content of stack
	void LoadStack();

	// Load entry of the auxiliary vector
	void LoadAVEntry(unsigned &sp, unsigned type, unsigned value);

	// Load auxiliary vector and return its size in bytes
	unsigned LoadAV(unsigned where);

	// Load dynamic linker
	void LoadInterp();

	///////////////////////////////////////////////////////////////////////
	//
	// Fields and functions related with signal handling. The functions are
	// implemented in ContextSignal.cc.
	//
	///////////////////////////////////////////////////////////////////////

	// Table of signal handlers, possibly shared by multiple contexts
	std::shared_ptr<SignalHandlerTable> signal_handler_table;

	// Table of signal masks, each context has its own
	SignalMaskTable signal_mask_table;

	// Run a signal handler for signal \a sig. The value of \a sig must be
	// between 1 and 64.
	void RunSignalHandler(int sig);

	///////////////////////////////////////////////////////////////////////
	//
	// Functions and fields related with mips instruction emulation,
	// implemented in ContextIsaXXX.cc files
	//
	///////////////////////////////////////////////////////////////////////

	// Prototype of a member function of class Context devoted to the
	// execution of ISA instructions. The emulator has a table indexed by an
	// instruction identifier that points to all instruction emulation
	// functions.
	typedef void (Context::*ExecuteInstFn)();

	// Instruction emulation functions. Each entry of Inst.def will be
	// expanded into a function prototype. For example, entry
	// 	DEFINST(J,"j%target",0x02,0x00,0x00,0x00)
	// is expanded to
	//	void ExecuteInst_adc_al_imm8();
#define DEFINST(_name, _fmt_str, _op0, _op1, _op2, _op3) void ExecuteInst_##_name();
#include <arch/mips/asm/Inst.def>
#undef DEFINST

	// Table of functions
	ExecuteInstFn execute_inst_fn[InstOpcodeCount];


public:
	/// Position of the context in the main context list. This field is
	/// managed by the emulator. When a context is removed from the main
	/// context list, it is automatically freed.
	std::list<std::unique_ptr<Context>>::iterator contexts_iter;

	/// Flag indicating whether this context is present in a certain context
	/// list of the emulator. This field is exclusively managed by the
	/// emulator.
	bool context_list_present[ContextListCount];

	/// Position of the context in a certain context list. This field is
	/// exclusively managed by the emulator.
	std::list<Context *>::iterator context_list_iter[ContextListCount];

	/// Constructor
	Context();

	/// Destructor
	~Context();

	void Load(const std::vector<std::string> &args,
			const std::vector<std::string> &env,
			const std::string &cwd,
			const std::string &stdin_file_name,
			const std::string &stdout_file_name);

	/// Given a file name, return its full path based on the current working
	/// directory for the context.
	std::string getFullPath(const std::string &path)
	{
		return misc::getFullPath(path, loader->cwd);
	}

	/// Run one instruction for the context at the position pointed to by
	/// register program counter.
	void Execute();

	/// Return \c true if flag \a state is part of the context state
	bool getState(ContextState state) const { return this->state & state; }

	/// Set flag \a state in the context state
	void setState(ContextState state) { UpdateState(this->state | state); }

	/// Clear flag \a state in the context state
	void clearState(ContextState state) { UpdateState(this->state
			& ~state); }

	/// Check whether a context suspended with a call to Suspend() is ready
	/// to wake up, by invoking the 'can_wakeup' callback.
	bool CanWakeup();

	/// Wake up a context in suspended state that went to sleep with a call
	/// to Suspend(). The 'wakeup_fn' callback function is invoked, and the
	/// wakeup data is internally freed by reseting the smart pointer.
	void Wakeup();

	// Check whether there is any pending unblocked signal in the context,
	// and invoke the corresponding signal handler.
	void CheckSignalHandler();
};

}  // namespace MIPS

#endif
