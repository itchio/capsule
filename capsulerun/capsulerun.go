package main

import (
	"log"
	"os"

	"github.com/fasterthanlime/gmf"
)

func main() {
	if len(os.Args) < 2 {
		log.Fatal("USAGE: capsulerun PROGRAM [ARGS]")
	}

	vCodec, err := gmf.FindEncoder(gmf.AV_CODEC_ID_H264)
	if err != nil {
		log.Fatal("Could not find H264 codec: ", err.Error())
	}
	log.Println("Found H264 codec")
	log.Println("Long name: ", vCodec.LongName())

	aCodec, err := gmf.FindEncoder("aac")
	if err != nil {
		log.Fatal("Could not find AAC codec: ", err.Error())
	}
	log.Println("Found AAC codec")
	log.Println("Long name: ", aCodec.LongName())

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
