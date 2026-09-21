// Copyright (c) 2025 The BoringSSL Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

//go:build ignore

// analyze_bssl_speed analyzes the JSON-formatted output of bssl speed
// and derives performance components in ns/op and ns/KiB by linear regression.
package main

import (
	"encoding/json"
	"fmt"
	"io"
	"log"
	"maps"
	"math"
	"os"
	"regexp"
	"slices"
	"strings"
	"time"
)

type record struct {
	Description  string
	NumCalls     int
	Microseconds int
	BytesPerCall int
}

type benchmarks []record

type benchmarkGroup []record

type groupedBenchmarks map[string]benchmarkGroup

var removeRE = regexp.MustCompile(` \(\d+ bytes?\)`)

func (b benchmarks) group() groupedBenchmarks {
	g := groupedBenchmarks{}
	for _, r := range b {
		desc := removeRE.ReplaceAllString(r.Description, "")
		g[desc] = append(g[desc], r)
	}
	return g
}

type regression struct {
	dataPoints  int
	constant    float64
	perKiB      float64
	coefficient float64
	err         error
}

// String returns a nice string representation of this.
func (r regression) String() string {
	if r.err != nil {
		return r.err.Error()
	}
	return fmt.Sprintf("%v + n * %v/KiB (n=%v, r=%v)",
		time.Duration(float64(time.Second)*r.constant+0.5),
		time.Duration(float64(time.Second)*r.perKiB+0.5),
		r.dataPoints,
		r.coefficient)
}

type regressions map[string]regression

func (g benchmarkGroup) regress() regression {
	if len(g) < 2 {
		return regression{
			err: fmt.Errorf("not enough datapoints: got %v, want >= 2", len(g)),
		}
	}
	var sx, sxx, sxy, sy, syy float64
	for _, r := range g {
		x := float64(r.BytesPerCall)
		y := float64(r.Microseconds) / float64(r.NumCalls) * 1e-6
		sx += x
		sxx += x * x
		sxy += x * y
		sy += y
		syy += y * y
	}
	n := float64(len(g))
	d1 := n*sxx - sx*sx
	if d1 == 0 {
		return regression{
			err: fmt.Errorf("all x values are equal: %v", sy/n),
		}
	}
	n1 := n*sxy - sx*sy
	m := n1 / d1
	b := (sy*sxx - sx*sxy) / d1
	r := 1.0
	d2 := n*syy - sy*sy
	if d2 != 0 {
		r = n1 / math.Sqrt(d1*d2)
	}
	return regression{
		dataPoints:  len(g),
		constant:    b,
		perKiB:      m * 1024,
		coefficient: r,
	}
}

func (b groupedBenchmarks) regress() regressions {
	r := regressions{}
	for d, g := range b {
		r[d] = g.regress()
	}
	return r
}

// String returns a nice string representation of this.
func (r regressions) String() string {
	var ret []string
	for _, k := range slices.Sorted(maps.Keys(r)) {
		ret = append(ret, fmt.Sprintf("%v: %v", k, r[k]))
	}
	return strings.Join(ret, "\n")
}

func main() {
	j := json.NewDecoder(os.Stdin)
	var b benchmarks
	for {
		var bnew benchmarks
		err := j.Decode(&bnew)
		if err == io.EOF {
			break
		}
		if err != nil {
			log.Panicf("failed decoding: %v", err)
		}
		b = append(b, bnew...)
	}
	fmt.Printf("%v\n", b.group().regress())
}
