package main

import (
	"log"

	"github.com/itchio/capsule/capsulerun/capsule"
)

func main() {
	must(capsule.Run())
}

func must(err error) {
	if err != nil {
		log.Fatalf("%+v", err)
	}
}
