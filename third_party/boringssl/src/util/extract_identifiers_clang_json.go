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

// extract_identifiers_clang_json parses the BoringSSL public includes and (for now)
// outputs a report of all identifiers defined therein. Sample usage:
//
// for f in include/openssl/*.h; do echo "#include <${f#include/}>"; done |\
//   clang++ -x c++ -std=c++17 -Iinclude -fsyntax-only -Xclang -ast-dump=json - \
//   go run util/extract_identifiers_clang_json.go > extract_identifiers.txt
//
// Note that right now the output of this tool is for human use only.
// The tool will likely be changed further for the purpose of symbol prefixing
// and auditing thereof.

package main

import (
	"encoding/json"
	"flag"
	"fmt"
	"log"
	"os"
	"path"
	"regexp"
	"strings"
)

var (
	dumpTree          = flag.Bool("dump_tree", false, "dump syntax tree while processing")
	dumpFullTree      = flag.Bool("dump_full_tree", false, "dump syntax tree while processing including system headers")
	keepGoing         = flag.Bool("keep_going", false, "continue even after errors")
	language          = flag.String("language", "C", "language to consider the source to be")
	globalSymbolsOnly = flag.Bool("global_symbols_only", false, "only output a list of names that may become (part of) linker symbols and are not namespaced")
)

// node is a node from the Clang AST dump.
type node struct {
	Kind  string
	Loc   loc
	Range rangeStruct `json:",omitempty"`
	Inner []*node     `json:",omitempty"`

	// Node fields that may or may not matter depending on `Kind`.
	CompleteDefinition bool        `json:",omitempty"`
	ConstExpr          bool        `json:",omitempty"`
	Decl               *node       `json:",omitempty"`
	IsImplicit         bool        `json:",omitempty"`
	Language           string      `json:",omitempty"`
	Name               string      `json:",omitempty"`
	PreviousDecl       string      `json:",omitempty"`
	StorageClass       string      `json:",omitempty"`
	TagUsed            string      `json:",omitempty"`
	Type               *typeStruct `json:",omitempty"`
}

// typeStruct is the type information from the Clang AST dump.
type typeStruct struct {
	QualType          string `json:",omitempty"`
	DesugaredQualType string `json:",omitempty"`
}

// desugaredQualType returns the canonical type after expanding typedefs.
func (t typeStruct) desugaredQualType() string {
	if t.DesugaredQualType != "" {
		return t.DesugaredQualType
	}
	return t.QualType
}

// constTypeRE is an approximate regex for types that are const.
// It errs on the side of returning constants as non-const.
var constType = regexp.MustCompile(`.*\bconst\b[^\[\]()*&]*$`)

// isConst returns whether the given type is likely a constant.
func (t typeStruct) isConst() bool {
	return constType.MatchString(t.desugaredQualType())
}

// rangeStruct is a location range from the Clang AST dump.
type rangeStruct struct {
	Begin *loc `json:",omitempty"`
	End   *loc `json:",omitempty"`
}

// loc is a location from the Clang AST dump.
type loc struct {
	File         string `json:",omitempty"`
	Line         uint   `json:",omitempty"`
	Col          uint   `json:",omitempty"`
	Offset       uint   `json:",omitempty"`
	TokLen       uint   `json:",omitempty"`
	SpellingLoc  *loc   `json:",omitempty"`
	ExpansionLoc *loc   `json:",omitempty"`
}

// expansionLoc returns the expansion location of the given loc.
func (l loc) expansionLoc() loc {
	if l.ExpansionLoc != nil {
		return *l.ExpansionLoc
	}
	if l.SpellingLoc != nil {
		return *l.SpellingLoc
	}
	return l
}

type decompressCtx struct {
	file string
	line uint
}

// decompress undoes the filename field compression from
// JSONNodeDumper::writeSourceLocation and JSONNodeDumper::writeBareSourceLocation.
func (l *loc) decompress(last *decompressCtx) {
	if l == nil {
		return
	}
	l.SpellingLoc.decompress(last)
	l.ExpansionLoc.decompress(last)
	if l.SpellingLoc != nil || l.ExpansionLoc != nil {
		return
	}
	if l.File == "" {
		l.File = last.file
	} else {
		last.file = l.File
	}
	if l.Line == 0 {
		l.Line = last.line
	} else {
		last.line = l.Line
	}
}

// path returns the loc's file path in clean, unique form.
func (l loc) path() string {
	return path.Clean(l.File)
}

// decompressLocsInternal is a helper for decompressLocs.
//
// It keeps state in its lastFile pointer.
func (n *node) decompressLocsInternal(last *decompressCtx) {
	n.Loc.decompress(last)
	n.Range.Begin.decompress(last)
	n.Range.End.decompress(last)
	for _, child := range n.Inner {
		child.decompressLocsInternal(last)
	}
}

// decompressLocs decompresses all Loc fields below a node.
//
// Should be called right after parsing.
func (n *node) decompressLocs() {
	n.decompressLocsInternal(&decompressCtx{})
}

// storage represents the storage class of a node.
type storage int

const (
	noStorage storage = iota
	externStorage
	staticStorage
)

// storage finds the storage class of the node.
func (n node) storage(language string) (storage, error) {
	var storage storage
	switch n.StorageClass {
	case "":
		if n.Kind == "VarDecl" && (n.ConstExpr || (language == "C++" && n.Type.isConst())) {
			storage = staticStorage
		} else {
			storage = externStorage
		}
	case "extern":
		storage = externStorage
	case "static":
		storage = staticStorage
	default:
		return noStorage, fmt.Errorf("no handling for storage class %q", n.StorageClass)
	}
	return storage, nil
}

// functionArgs returns all the function arg nodes of a node.
func (n node) functionArgs() []*node {
	var result []*node
	for _, child := range n.Inner {
		if child.Kind == "ParmVarDecl" {
			result = append(result, child)
		}
	}
	return result
}

func (n node) locationRange() (file string, line, start, end uint, ok bool) {
	from := n.Range.Begin.expansionLoc()
	to := n.Range.End.expansionLoc()
	if from.path() != to.path() {
		return "", 0, 0, 0, false
	}
	return from.path(), from.Line, from.Offset, to.Offset + to.TokLen, true
}

func (n node) locationRangeWithoutBody() (file string, line, start, end uint, ok bool) {
	file, line, start, end, ok = n.locationRange()
	if !ok {
		return
	}
	// Cut bad nodes from the end.
	for i := len(n.Inner) - 1; i >= 0; i-- {
		child := n.Inner[i]
		if child.Kind != "CompoundStmt" {
			continue
		}
		cfile, _, cstart, cend, ok := child.locationRange()
		if !ok {
			continue
		}
		if cfile == file && cend == end {
			end = cstart
		}
	}
	return
}

// namespacing indicates how the identifier respects namespaces.
type namespacing int

const (
	alwaysGlobal     namespacing = iota // Never in namespace (such as preprocessor macros).
	globalIfC                           // Respects namespace unless in extern "C" (such as functions).
	alwaysNamespaced                    // Always respects namespace (such as types).
)

// linking indicates how the identifier responds to extern "C" or similar.
type linking int

const (
	neverLinked     linking = iota // Ignores linkage information (such as types).
	respectsLinkage                // Respects linkage information (such as functions).
)

// walker is data that is transported to inner nodes while parsing.
type walker struct {
	*walkerStatic // Data that can be mutated even by downstream nodes.

	inBoringSSL   bool     // Whether the code originates from BoringSSL.
	depth         int      // Nesting depth (for -dump_tree output).
	namespace     []string // C++ namespace sequence the node is in.
	anonNamespace bool     // Whether the node is in a C++ anonymous namespace.
	language      string   // Can be "C" or "C++".
	record        bool     // Whether the current node is part of a record.
}

// walkerStatic is data that is transported in reading direction while parsing.
type walkerStatic struct {
	seen map[string]string // All identifiers seen so far.
}

func newWalker() walker {
	return walker{
		walkerStatic: &walkerStatic{
			seen: map[string]string{},
		},
		language: *language,
	}
}

// Consider files with a non-absolute path to be BoringSSL,
// whereas absolute paths usually indicate system header locations.
//
// Note that any non-word character in the first two characters is treated as
// indicating an absolute path to catch "<built-in>", "/foo/bar.h" and "C:\foo\bar.h".
var (
	boringSSLPath = regexp.MustCompile(`^\w\w`)
)

// updateInBoringSSL checks whether the given directive is a file/line directive,
// and if so, checks if it's likely part of BoringSSL or not.
//
// The return value indicates whether it's a file/line directive.
// If it is, `*in` will be updated to the current status of whether this is BoringSSL.
func (w *walker) updateInBoringSSL(kind string, loc loc) {
	if kind == "TranslationUnitDecl" {
		w.inBoringSSL = true
		return
	}
	w.inBoringSSL = boringSSLPath.MatchString(loc.expansionLoc().path())
}

// visit traverses a node in the AST and analyzes it for identifiers contained therein.
func (w walker) visit(n *node) (err error) {
	nodeWithoutChildren := *n
	nodeWithoutChildren.Inner = nil
	nodeCode, err := json.Marshal(nodeWithoutChildren)
	if err != nil {
		return err
	}

	if (*dumpTree && w.inBoringSSL) || *dumpFullTree {
		log.Printf("%*s[%s] %s: %s (%d children)",
			w.depth, "",
			strings.Join(w.namespace, "::"),
			n.Kind,
			nodeCode,
			len(n.Inner))
	}

	// Allow to ignore errors.
	defer func() {
		if *keepGoing && err != nil {
			log.Printf("ERROR: %v", err)
			err = nil
		}
	}()

	// Update "w".
	w.depth++

	// Update "in BoringSSL".
	w.updateInBoringSSL(n.Kind, n.Loc)

	if !w.inBoringSSL || n.IsImplicit {
		// If suppressed, below nodes are not interesting.
		// Also, skip any non-BoringSSL code such as system headers.
		return nil
	}

	switch n.Kind {
	// Nodes that need handling.
	case "CXXRecordDecl", "RecordDecl":
		if w.record && n.CompleteDefinition {
			return nil
		}
		if n.Name != "" {
			if err := w.collectIdentifier(n, n.TagUsed, alwaysNamespaced, neverLinked, noStorage); err != nil {
				return err
			}
		}
		w.record = true
	case "EnumDecl":
		if w.record {
			return nil
		}
		if n.Name != "" {
			if err := w.collectIdentifier(n, "enum", alwaysNamespaced, neverLinked, noStorage); err != nil {
				return err
			}
		}
	case "EnumConstantDecl":
		if w.record {
			return nil
		}
		if err := w.collectIdentifier(n, "enumerator", alwaysNamespaced, neverLinked, noStorage); err != nil {
			return err
		}
		return nil // Do not recurse.
	case "FunctionDecl":
		if w.record {
			return nil
		}
		if n.PreviousDecl != "" {
			return // Definition or redeclaration doesn't need to be looked at again (and may have incomplete qualifiers).
		}
		storage, err := n.storage(w.language)
		if err != nil {
			return fmt.Errorf("could not find storage class of function: %w: %s", err, nodeCode)
		}
		if err := w.collectIdentifier(n, "function", globalIfC, respectsLinkage, storage); err != nil {
			return err
		}
	case "LinkageSpecDecl":
		if n.Language != "" {
			w.language = n.Language
		}
	case "NamespaceDecl":
		if w.language != "C++" {
			return fmt.Errorf("entering namespace while in extern %q is probably unintended: %s", w.language, nodeCode)
		}
		if n.Name == "" {
			w.anonNamespace = true
		} else {
			w.namespace = append(append([]string(nil), w.namespace...), n.Name)
		}
	case "TypeAliasDecl", "TypeAliasTemplateDecl":
		if w.record {
			return nil
		}
		if err := w.collectIdentifier(n, "using", alwaysNamespaced, neverLinked, noStorage); err != nil {
			return err
		}
	case "TypedefDecl":
		if w.record {
			return nil
		}
		if len(n.Inner) == 1 && n.Inner[0].Kind == "ElaboratedType" && len(n.Inner[0].Inner) == 1 && n.Inner[0].Inner[0].Decl != nil && n.Inner[0].Inner[0].Decl.Name == n.Name {
			// typedef struct X X;
			return nil
		}
		if err := w.collectIdentifier(n, "typedef", alwaysNamespaced, neverLinked, noStorage); err != nil {
			return err
		}
	case "VarDecl":
		if n.PreviousDecl != "" {
			return // Definition or redeclaration doesn't need to be looked at again (and may have incomplete qualifiers).
		}
		storage, err := n.storage(w.language)
		if err != nil {
			return fmt.Errorf("could not find storage class of variable: %w: %s", err, nodeCode)
		}
		if err := w.collectIdentifier(n, "var", globalIfC, respectsLinkage, storage); err != nil {
			return err
		}
		return nil // Do not recurse. (Maybe should, to catch `struct ...` in variable types?)
	// Singletons that should be skipped.
	case
		"AccessSpecDecl",
		"AlwaysInlineAttr",
		"BuiltinAttr",
		"BuiltinType",
		"CXX11NoReturnAttr",
		"ConstAttr",
		"DependentNameType",
		"DeprecatedAttr",
		"EnumType",
		"FinalAttr",
		"FormatAttr",
		"NoThrowAttr",
		"RecordType",
		"TemplateTypeParmType",
		"UnresolvedUsingValueDecl",
		"UnusedAttr",
		"UsingDecl",
		"UsingDirectiveDecl",
		"VisibilityAttr",
		"WarnUnusedResultAttr":
		if len(n.Inner) != 0 {
			// If this ever fires, check AST to see if any of the node's children could be useful,
			// then categorize the node type into one of the following two cases.
			return fmt.Errorf("singleton node of kind %q has children: %s", n.Kind, nodeCode)
		}
	// Nodes that should be skipped including possible children.
	case
		"AlignedAttr",
		"CXXConstructorDecl",
		"CXXConversionDecl",
		"CXXDeductionGuideDecl",
		"CXXDestructorDecl",
		"CXXMethodDecl",
		"ClassTemplatePartialSpecializationDecl",
		"ClassTemplateSpecializationDecl",
		"CompoundStmt",
		"DecltypeType",
		"FieldDecl",
		"FriendDecl",
		"NonTypeTemplateParmDecl",
		"ParmVarDecl",
		"StaticAssertDecl",
		"TemplateArgument",
		"TemplateTemplateParmDecl",
		"TemplateTypeParmDecl",
		"VarTemplateDecl",
		"VarTemplateSpecializationDecl":
		return nil // Do not recurse.
	// Nodes that should just be recursed into.
	case
		"ClassTemplateDecl",
		"ConstantArrayType",
		"DecayedType",
		"ElaboratedType",
		"FunctionProtoType",
		"FunctionTemplateDecl",
		"IncompleteArrayType",
		"IndirectFieldDecl",
		"LValueReferenceType",
		"ParenType",
		"PointerType",
		"QualType",
		"TemplateSpecializationType",
		"TranslationUnitDecl",
		"TypedefType",
		"VectorType":
		// Just recurse.
	default:
		return fmt.Errorf("no handling for node kind %q: %s", n.Kind, nodeCode)
	}

	// If we get here (via fallthrough usually), we want to recurse.
	// To avoid recursing, use return.
	for _, child := range n.Inner {
		err = w.visit(child)
		if err != nil {
			break
		}
	}

	return err
}

// collectIdentifier sends an identifier to the output.
func (w walker) collectIdentifier(n *node, tag string, namespacing namespacing, linking linking, storage storage) error {
	name := n.Name
	var fqn string
	if w.anonNamespace {
		fqn = "<anonymous>::" + name
	} else {
		fqn = strings.Join(append(append([]string(nil), w.namespace...), name), "::")
	}

	// With this taken out of the way, for all intents and purposes
	// anything in an anonymous namespace behaves as if it were static.
	//
	// Helps in some cases where the static keyword
	// isn't repeated in template specializations or similar.
	if w.anonNamespace && w.language == "C++" {
		storage = staticStorage
	}

	var linkage string
	switch linking {
	case neverLinked:
		linkage = ""
	case respectsLinkage:
		switch storage {
		case externStorage:
			linkage = fmt.Sprintf("extern %q ", w.language)
		case staticStorage:
			linkage = "static "
		default:
			return fmt.Errorf("respecting linkage, but storage not set for %v", fqn)
		}
	}

	var namespaced bool
	switch namespacing {
	case alwaysGlobal:
		namespaced = false
	case globalIfC:
		namespaced = w.language == "C++"
	case alwaysNamespaced:
		namespaced = true
	}

	var identifier string
	if namespaced {
		identifier = fqn
	} else {
		identifier = name
	}

	declaration := fmt.Sprintf("%s%s %s;", linkage, tag, identifier)
	key := identifier
	seen, found := w.seen[key]
	if found {
		if seen != declaration {
			return fmt.Errorf("duplicate distinct definition of %v: %v and %v", key, seen, declaration)
		}
		return nil
	}
	w.seen[key] = declaration

	if *globalSymbolsOnly {
		if !namespaced && storage != staticStorage {
			fmt.Printf("%s\n", identifier)
		}
		return nil
	}

	// Append some debug info.
	// This might be used later to generate symbol renaming headers,
	// but is generally useful to humans debugging this tool's output.
	var suffix string
	if file, line, start, end, ok := n.locationRangeWithoutBody(); ok {
		suffix += fmt.Sprintf(" %s:%v (%v-%v)", file, line, start, end)
	}
	for _, arg := range n.functionArgs() {
		argName := arg.Name
		if argName == "" {
			argName = "_"
		}
		suffix += " " + argName
	}
	if suffix != "" {
		suffix = "  //" + suffix
	}
	fmt.Printf("%s%s\n", declaration, suffix)
	return nil
}

// Main is the main program.
func Main() error {
	j := json.NewDecoder(os.Stdin)

	w := newWalker()

	for j.More() {
		var root node
		err := j.Decode(&root)
		if err != nil {
			return err
		}
		root.decompressLocs()
		err = w.visit(&root)
		if err != nil {
			return err
		}
	}

	return nil
}

// main runs Main turning errors into exit codes.
func main() {
	flag.Parse()
	err := Main()
	if err != nil {
		log.Panicf("error returned from Main: %v", err)
	}
}
