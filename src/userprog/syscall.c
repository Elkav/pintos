#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "devices/shutdown.h"


static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

// Helper for erroring on validations - sets the thread exit status to -1 and triggers the process_exit message
static void exit_with_error() {
  struct thread *cur = thread_current();
  cur->exit_status = -1;
  thread_exit();
}

// Validate one byte addr
static void validate_user_ptr(const void *vaddr) {
  // If it's a null pointer/points to kernel memory/isn't a user vaddr, error
  if (vaddr == NULL || !is_user_vaddr(vaddr)) {
    exit_with_error();
  }

  
  // // Check if the page is actually mapped in the process's page directory
  struct thread *cur = thread_current();
  void *kaddr = pagedir_get_page(cur->pagedir, vaddr);
  if (kaddr == NULL) {
    exit_with_error();
  }
}

// Validate a range of bytes
static void validate_user_range(const void *buff, unsigned size) {
  for (unsigned i = 0; i < size; i++) {
    validate_user_ptr((const uint8_t *)buff + i);
  }
}

// Safely fetch 32-bit word from the user stack
static uint32_t copy_in_u32(const void *addr) {
  validate_user_range(addr, 4); // 32 bits = 4 bytes
  return *((uint32_t *)addr);
}


int syscall_write(int fd, const void *buffer, unsigned size) {
  /* For simplicity, we only handle writing to stdout (fd = 1). */
  if (fd != 1) return -1;
  /* Write to console output. */
  putbuf (buffer, size);
  return size;
}

static void syscall_handler(struct intr_frame *f UNUSED){

  // Safely grab the syscall number from the stack pointer
  uint32_t syscall_no = copy_in_u32(f->esp);

  switch (syscall_no) {
    case SYS_HALT: { // SYS_HALT terminates Pintos
      shutdown_power_off();
      break;
    }
    case SYS_EXIT: {
      // Safely read the exit status (the 1st argument, 4 bytes offset from esp)
      int status = (int) copy_in_u32((uint8_t *)f->esp + 4);

      // Set the current thread's exit status to this value
      struct thread *cur = thread_current();
      cur->exit_status = status;
      thread_exit();
      break;
    }

    case SYS_WRITE: {
      // Safely read arguments 1, 2, and 3
      int fd = (int) copy_in_u32((uint8_t *)f->esp + 4);
      const void *buffer = (const void *) copy_in_u32((uint8_t *)f->esp + 8);
      unsigned size = (unsigned) copy_in_u32((uint8_t *)f->esp + 12);

      validate_user_range(buffer, size); // Ensure the buffer is readable
      f->eax = syscall_write(fd, buffer, size);
      break;
    }

    default: {
      // for unknown syscalls just terminate the process
      thread_exit();
      break;
    }
  }
}