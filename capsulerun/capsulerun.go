package main

import (
	"log"
	"os"
)

func main() {
	if len(os.Args) < 2 {
		log.Fatal("USAGE: capsulerun PROGRAM [ARGS]")
	}

	program := os.Args[1]
	args := os.Args[1:]

	log.Printf("Capsule: running %s %v", program, args)

	procAttr := &os.ProcAttr{
		Files: []*os.File{os.Stdin, os.Stdout, os.Stderr},
	}
	process, err := os.StartProcess(program, args, procAttr)
	if err != nil {
		log.Fatal("Could not start process: ", err.Error())
	}

	process.Wait()
}
