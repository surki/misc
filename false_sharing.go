package main

import (
	"flag"
	"fmt"
	"os"
	"runtime"
	"sync"
	"sync/atomic"
	"syscall"
	"unsafe"

	"golang.org/x/sys/unix"
)

var usageTxt string = `A simple example program to measure cacheline false sharing using 'perf c2c' tool

Typical example:
  sudo perf c2c record -F 60000 --all-user -- ./false_sharing
  sudo perf c2c report -c pid,iaddr --full-symbols --stdio -d lcl

Parameters:

`

func main() {

	flag.Usage = func() {
		fmt.Fprintf(os.Stderr, usageTxt)
		flag.PrintDefaults()
	}

	useLocal := flag.Bool("local", false, "enable local variable (== no cache contention)")
	pinCPU := flag.Bool("pinToCPU", false, "Lock goroutine to OS thread and pin them to specific CPU(s)")

	flag.Parse()
	fmt.Println("Starting")
	if *useLocal {
		fmt.Println("Using local variable (== no cache contention)")
	} else {
		fmt.Println("Using global variable (== more contention)")
	}

	var ops uint64 = 0

	var wg sync.WaitGroup

	for i := 0; i < runtime.GOMAXPROCS(0); i++ {
		wg.Add(1)
		go func(i int) {
			defer wg.Done()

			if *pinCPU {
				pinToCPU(uint(i))
			}

			var localOps uint64 = 0
			var opsPtr *uint64

			if *useLocal {
				opsPtr = &localOps
			} else {
				opsPtr = &ops
			}

			for i := 0; i < 20000000; i++ {
				atomic.AddUint64(opsPtr, 1)
			}

			if *useLocal {
				atomic.AddUint64(&ops, localOps)
			}
		}(i)
	}

	wg.Wait()

	opsFinal := atomic.LoadUint64(&ops)
	fmt.Println("ops:", opsFinal)
}

func pinToCPU(cpu uint) error {
	var mask [1024 / 64]uint8
	runtime.LockOSThread()
	mask[cpu/64] |= 1 << (cpu % 64)
	_, _, errno := syscall.RawSyscall(unix.SYS_SCHED_SETAFFINITY, 0, uintptr(len(mask)*8), uintptr(unsafe.Pointer(&mask)))
	if errno != 0 {
		return errno
	}
	return nil
}
