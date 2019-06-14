// RUN: mlir-opt %s -split-input-file -verify

// Check different error cases.
// -----

func @illegaltype(i) // expected-error {{expected non-function type}}

// -----

func @illegaltype() {
  %0 = constant dense<<vector 4 x f32>, 0> : vector<4 x f32> // expected-error {{expected non-function type}}
}

// -----

func @nestedtensor(tensor<tensor<i8>>) -> () // expected-error {{invalid tensor element type}}

// -----

func @indexvector(vector<4 x index>) -> () // expected-error {{vector elements must be int or float type}}

// -----

// Everything is valid in a memref.
func @indexmemref(memref<? x index>) -> ()

// -----

func @indextensor(tensor<4 x index>) -> () // expected-error {{invalid tensor element type}}

// -----
// Test no map in memref type.
func @memrefs(memref<2x4xi8, >) // expected-error {{expected list element}}

// -----
// Test non-existent map in memref type.
func @memrefs(memref<2x4xi8, #map7>) // expected-error {{undefined symbol alias id 'map7'}}

// -----
// Test non affine map in memref type.
func @memrefs(memref<2x4xi8, i8>) // expected-error {{expected affine map in memref type}}

// -----
// Test non-existent map in map composition of memref type.
#map0 = (d0, d1) -> (d0, d1)

func @memrefs(memref<2x4xi8, #map0, #map8>) // expected-error {{undefined symbol alias id 'map8'}}

// -----
// Test multiple memory space error.
#map0 = (d0, d1) -> (d0, d1)
func @memrefs(memref<2x4xi8, #map0, 1, 2>) // expected-error {{multiple memory spaces specified in memref type}}

// -----
// Test affine map after memory space.
#map0 = (d0, d1) -> (d0, d1)
#map1 = (d0, d1) -> (d0, d1)

func @memrefs(memref<2x4xi8, #map0, 1, #map1>) // expected-error {{affine map after memory space in memref type}}

// -----
// Test dimension mismatch between memref and layout map.
// The error must be emitted even for the trivial identity layout maps that are
// dropped in type creation.
#map0 = (d0, d1) -> (d0, d1)
func @memrefs(memref<42xi8, #map0>) // expected-error {{memref affine map dimension mismatch}}

// -----

#map0 = (d0, d1) -> (d0, d1)
#map1 = (d0) -> (d0)
func @memrefs(memref<42x42xi8, #map0, #map1>) // expected-error {{memref affine map dimension mismatch}}

// -----

func missingsigil() -> (i1, index, f32) // expected-error {{expected a function identifier like}}


// -----

func @bad_branch() {
^bb12:
  br ^missing  // expected-error {{reference to an undefined block}}
}

// -----

func @block_redef() {
^bb42:
  return
^bb42:        // expected-error {{redefinition of block '^bb42'}}
  return
}

// -----

func @no_terminator() {   // expected-error {{block with no terminator}}
^bb40:
  return
^bb41:
^bb42:
  return
}

// -----

func @block_no_rparen() {
^bb42 (%bb42 : i32: // expected-error {{expected ')' to end argument list}}
  return
}

// -----

func @block_arg_no_ssaid() {
^bb42 (i32): // expected-error {{expected SSA operand}}
  return
}

// -----

func @block_arg_no_type() {
^bb42 (%0): // expected-error {{expected ':' and type for SSA operand}}
  return
}

// -----

func @block_arg_no_close_paren() {
^bb42:
  br ^bb2( // expected-error@+1 {{expected ')' to close argument list}}
  return
}

// -----

func @block_first_has_predecessor() {
// expected-error@-1 {{entry block of region may not have predecessors}}
^bb42:
  br ^bb43
^bb43:
  br ^bb42
}

// -----

func @empty() { // expected-error {{function must have a body}}
}

// -----

func @no_return() {
  %x = constant 0 : i32  // expected-error {{block with no terminator}}
}

// -----

"       // expected-error {{expected}}
"

// -----

"       // expected-error {{expected}}

// -----

func @bad_op_type() {
^bb40:
  "foo"() : i32  // expected-error {{expected function type}}
  return
}
// -----

func @no_terminator() {
^bb40:
  "foo"() : ()->()
  ""() : ()->()  // expected-error {{empty operation name is invalid}}
  return
}

// -----

func @illegaltype(i0) // expected-error {{invalid integer width}}

// -----

func @malformed_for_percent() {
  affine.for i = 1 to 10 { // expected-error {{expected SSA operand}}

// -----

func @malformed_for_equal() {
  affine.for %i 1 to 10 { // expected-error {{expected '='}}

// -----

func @malformed_for_to() {
  affine.for %i = 1 too 10 { // expected-error {{expected 'to' between bounds}}
  }
}

// -----

func @incomplete_for() {
  affine.for %i = 1 to 10 step 2
}        // expected-error {{expected '{' to begin a region}}

// -----

#map0 = (d0) -> (d0 floordiv 4)

func @reference_to_iv_in_bound() {
  // expected-error@+1 {{operand use before it's defined}}
  affine.for %i0 = #map0(%i0) to 10 {
  }
}

// -----

func @nonconstant_step(%1 : i32) {
  affine.for %2 = 1 to 5 step %1 { // expected-error {{expected non-function type}}

// -----

func @for_negative_stride() {
  affine.for %i = 1 to 10 step -1
}        // expected-error@-1 {{expected step to be representable as a positive signed integer}}

// -----

func @non_operation() {
  asd   // expected-error {{custom op 'asd' is unknown}}
}

// -----

func @invalid_if_conditional2() {
  affine.for %i = 1 to 10 {
    affine.if (i)[N] : (i >= )  // expected-error {{expected '== 0' or '>= 0' at end of affine constraint}}
  }
}

// -----

func @invalid_if_conditional3() {
  affine.for %i = 1 to 10 {
    affine.if (i)[N] : (i == 1) // expected-error {{expected '0' after '=='}}
  }
}

// -----

func @invalid_if_conditional4() {
  affine.for %i = 1 to 10 {
    affine.if (i)[N] : (i >= 2) // expected-error {{expected '0' after '>='}}
  }
}

// -----

func @invalid_if_conditional5() {
  affine.for %i = 1 to 10 {
    affine.if (i)[N] : (i <= 0 ) // expected-error {{expected '== 0' or '>= 0' at end of affine constraint}}
  }
}

// -----

func @invalid_if_conditional6() {
  affine.for %i = 1 to 10 {
    affine.if (i) : (i) // expected-error {{expected '== 0' or '>= 0' at end of affine constraint}}
  }
}

// -----
// TODO (support affine.if (1)?
func @invalid_if_conditional7() {
  affine.for %i = 1 to 10 {
    affine.if (i) : (1) // expected-error {{expected '== 0' or '>= 0' at end of affine constraint}}
  }
}

// -----

#map = (d0) -> (%  // expected-error {{invalid SSA name}}

// -----

func @test() {
^bb40:
  %1 = "foo"() : (i32)->i64 // expected-error {{expected 0 operand types but had 1}}
  return
}

// -----

func @redef() {
^bb42:
  %x = "xxx"(){index: 0} : ()->i32 // expected-note {{previously defined here}}
  %x = "xxx"(){index: 0} : ()->i32 // expected-error {{redefinition of SSA value '%x'}}
  return
}

// -----

func @undef() {
^bb42:
  %x = "xxx"(%y) : (i32)->i32   // expected-error {{use of undeclared SSA value}}
  return
}

// -----

func @malformed_type(%a : intt) { // expected-error {{expected non-function type}}
}

// -----

func @resulterror() -> i32 {
^bb42:
  return    // expected-error {{'std.return' op has 0 operands, but enclosing function returns 1}}
}

// -----

func @func_resulterror() -> i32 {
  return // expected-error {{'std.return' op has 0 operands, but enclosing function returns 1}}
}

// -----

func @argError() {
^bb1(%a: i64):  // expected-note {{previously defined here}}
  br ^bb2
^bb2(%a: i64):  // expected-error{{redefinition of SSA value '%a'}}
  return
}

// -----

func @bbargMismatch(i32, f32) {
// expected-error @-1 {{first block of function must have 2 arguments to match function signature}}
^bb42(%0: f32):
  return
}

// -----

func @br_mismatch() {
^bb0:
  %0:2 = "foo"() : () -> (i1, i17)
  // expected-error @+1 {{branch has 2 operands, but target block has 1}}
  br ^bb1(%0#1, %0#0 : i17, i1)

^bb1(%x: i17):
  return
}

// -----

// Test no nested vector.
func @vectors(vector<1 x vector<1xi32>>, vector<2x4xf32>)
// expected-error@-1 {{vector elements must be int or float type}}

// -----

func @condbr_notbool() {
^bb0:
  %a = "foo"() : () -> i32 // expected-note {{prior use here}}
  cond_br %a, ^bb0, ^bb0 // expected-error {{use of value '%a' expects different type than prior uses}}
// expected-error@-1 {{expected condition type was boolean (i1)}}
}

// -----

func @condbr_badtype() {
^bb0:
  %c = "foo"() : () -> i1
  %a = "foo"() : () -> i32
  cond_br %c, ^bb0(%a, %a : i32, ^bb0) // expected-error {{expected non-function type}}
}

// -----

func @condbr_a_bb_is_not_a_type() {
^bb0:
  %c = "foo"() : () -> i1
  %a = "foo"() : () -> i32
  cond_br %c, ^bb0(%a, %a : i32, i32), i32 // expected-error {{expected block name}}
}

// -----

func @successors_in_non_terminator(%a : i32, %b : i32) {
  %c = "std.addi"(%a, %b)[^bb1] : () -> () // expected-error {{successors in non-terminator}}
^bb1:
  return
}

// -----

func @undef() {
^bb0:
  %x = "xxx"(%y) : (i32)->i32   // expected-error {{use of undeclared SSA value name}}
  return
}

// -----

func @undef() {
  %x = "xxx"(%y) : (i32)->i32   // expected-error {{use of undeclared SSA value name}}
  return
}

// -----

func @duplicate_induction_var() {
  affine.for %i = 1 to 10 {   // expected-note {{previously defined here}}
    affine.for %i = 1 to 10 { // expected-error {{redefinition of SSA value '%i'}}
    }
  }
  return
}

// -----

func @name_scope_failure() {
  affine.for %i = 1 to 10 {
  }
  "xxx"(%i) : (index)->()   // expected-error {{use of undeclared SSA value name}}
  return
}

// -----

func @dominance_failure() {
^bb0:
  "foo"(%x) : (i32) -> ()    // expected-error {{operand #0 does not dominate this use}}
  br ^bb1
^bb1:
  %x = "bar"() : () -> i32    // expected-note {{operand defined here}}
  return
}

// -----

func @return_type_mismatch() -> i32 {
  %0 = "foo"() : ()->f32
  return %0 : f32  // expected-error {{type of return operand 0 ('f32') doesn't match function result type ('i32')}}
}

// -----

func @return_inside_loop() {
  affine.for %i = 1 to 100 {
    // expected-error@-1 {{op expects regions to end with 'affine.terminator'}}
    // expected-note@-2 {{in custom textual format, the absence of terminator implies}}
    return
  }
  return
}

// -----

func @redef()
func @redef()  // expected-error {{redefinition of function named 'redef'}}

// -----

func @foo() {
^bb0:
  %x = constant @foo : (i32) -> ()  // expected-error {{reference to function with mismatched type}}
  return
}

// -----

func @undefined_function() {
^bb0:
  %x = constant @bar : (i32) -> ()  // expected-error {{reference to undefined function 'bar'}}
  return
}

// -----

func @invalid_result_type() -> () -> ()  // expected-error {{expected a top level entity}}

// -----

#map1 = (i)[j] -> (i+j)

func @bound_symbol_mismatch(%N : index) {
  affine.for %i = #map1(%N) to 100 {
  // expected-error@-1 {{symbol operand count and integer set symbol count must match}}
  }
  return
}

// -----

#map1 = (i)[j] -> (i+j)

func @bound_dim_mismatch(%N : index) {
  affine.for %i = #map1(%N, %N)[%N] to 100 {
  // expected-error@-1 {{dim operand count and integer set dim count must match}}
  }
  return
}

// -----

func @large_bound() {
  affine.for %i = 1 to 9223372036854775810 {
  // expected-error@-1 {{integer constant out of range for attribute}}
  }
  return
}

// -----

func @max_in_upper_bound(%N : index) {
  affine.for %i = 1 to max (i)->(N, 100) { //expected-error {{expected non-function type}}
  }
  return
}

// -----

func @step_typo() {
  affine.for %i = 1 to 100 step -- 1 { //expected-error {{expected constant integer}}
  }
  return
}

// -----

func @invalid_bound_map(%N : i32) {
  affine.for %i = 1 to (i)->(j)(%N) { //expected-error {{use of undeclared identifier}}
  }
  return
}

// -----

#set0 = (i)[N, M] : )i >= 0) // expected-error {{expected '(' at start of integer set constraint list}}

// -----
#set0 = (i)[N] : (i >= 0, N - i >= 0)

func @invalid_if_operands1(%N : index) {
  affine.for %i = 1 to 10 {
    affine.if #set0(%i) {
    // expected-error@-1 {{symbol operand count and integer set symbol count must match}}

// -----
#set0 = (i)[N] : (i >= 0, N - i >= 0)

func @invalid_if_operands2(%N : index) {
  affine.for %i = 1 to 10 {
    affine.if #set0()[%N] {
    // expected-error@-1 {{dim operand count and integer set dim count must match}}

// -----
#set0 = (i)[N] : (i >= 0, N - i >= 0)

func @invalid_if_operands3(%N : index) {
  affine.for %i = 1 to 10 {
    affine.if #set0(%i)[%i] {
    // expected-error@-1 {{operand cannot be used as a symbol}}
    }
  }
  return
}

// -----
// expected-error@+1 {{expected '"' in string literal}}
"J// -----
func @calls(%arg0: i32) {
  // expected-error@+1 {{expected non-function type}}
  %z = "casdasda"(%x) : (ppop32) -> i32
}
// -----
// expected-error@+2 {{expected SSA operand}}
func@n(){^b(
// -----

func @elementsattr_non_tensor_type() -> () {
^bb0:
  "foo"(){bar: dense<i32, [4]>} : () -> () // expected-error {{elements literal must be a ranked tensor or vector type}}
}

// -----

func @elementsattr_non_ranked() -> () {
^bb0:
  "foo"(){bar: dense<tensor<?xi32>, [4]>} : () -> () // expected-error {{elements literal type must have static shape}}
}

// -----

func @elementsattr_shape_mismatch() -> () {
^bb0:
  "foo"(){bar: dense<tensor<5xi32>, [4]>} : () -> () // expected-error {{inferred shape of elements literal ([1]) does not match type ([5])}}
}

// -----

func @elementsattr_invalid() -> () {
^bb0:
  "foo"(){bar: dense<tensor<2xi32>, [4, [5]]>} : () -> () // expected-error {{tensor literal is invalid; ranks are not consistent between elements}}
}

// -----

func @elementsattr_badtoken() -> () {
^bb0:
  "foo"(){bar: dense<tensor<1xi32>, [tf_opaque]>} : () -> () // expected-error {{expected element literal of primitive type}}
}

// -----

func @elementsattr_floattype1() -> () {
^bb0:
  // expected-error@+1 {{floating point value not valid for specified type}}
  "foo"(){bar: dense<tensor<1xi32>, [4.0]>} : () -> ()
}

// -----

func @elementsattr_floattype1() -> () {
^bb0:
  // expected-error@+1 {{floating point value not valid for specified type}}
  "foo"(){bar: dense<tensor<i32>, 4.0>} : () -> ()
}

// -----

func @elementsattr_floattype2() -> () {
^bb0:
  // expected-error@+1 {{integer value not valid for specified type}}
  "foo"(){bar: dense<tensor<1xf32>, [4]>} : () -> ()
}

// -----

func @elementsattr_toolarge1() -> () {
^bb0:
  "foo"(){bar: dense<tensor<1xi8>, [777]>} : () -> () // expected-error {{integer constant out of range for attribute}}
}

// -----

func @elementsattr_toolarge2() -> () {
^bb0:
  "foo"(){bar: dense<tensor<1xi8>, [-777]>} : () -> () // expected-error {{integer constant out of range for attribute}}
}

// -----

func @elementsattr_malformed_opaque() -> () {
^bb0:
  "foo"(){bar: opaque<tensor<1xi8>, "0xQZz123">} : () -> () // expected-error {{expected dialect namespace}}
}

// -----

func @elementsattr_malformed_opaque1() -> () {
^bb0:
  "foo"(){bar: opaque<"", tensor<1xi8>, "0xQZz123">} : () -> () // expected-error {{opaque string only contains hex digits}}
}

// -----

func @elementsattr_malformed_opaque2() -> () {
^bb0:
  "foo"(){bar: opaque<"", tensor<1xi8>, "00abc">} : () -> () // expected-error {{opaque string should start with '0x'}}
}

// -----

func @elementsattr_malformed_opaque3() -> () {
^bb0:
  "foo"(){bar: opaque<"t", tensor<1xi8>, "0xabc">} : () -> () // expected-error {{no registered dialect with namespace 't'}}
}

// -----

func @redundant_signature(%a : i32) -> () {
^bb0(%b : i32):  // expected-error {{invalid block name in region with named arguments}}
  return
}

// -----

func @mixed_named_arguments(%a : i32,
                               f32) -> () {
    // expected-error @-1 {{expected SSA identifier}}
  return
}

// -----

func @mixed_named_arguments(f32,
                               %a : i32) -> () { // expected-error {{expected type instead of SSA identifier}}
  return
}

// -----

// This used to crash the parser, but should just error out by interpreting
// `tensor` as operator rather than as a type.
func @f(f32) {
^bb0(%a : f32):
  %18 = cmpi "slt", %idx, %idx : index
  tensor<42 x index  // expected-error {{custom op 'tensor' is unknown}}
  return
}

// -----

func @f(%m : memref<?x?xf32>) {
  affine.for %i0 = 0 to 42 {
    // expected-error@+1 {{operand #2 does not dominate this use}}
    %x = load %m[%i0, %i1] : memref<?x?xf32>
  }
  affine.for %i1 = 0 to 42 {
  }
  return
}

// -----

func @dialect_type_empty_namespace(!<"">) -> () { // expected-error {{invalid type identifier}}
  return
}

// -----

func @dialect_type_no_string_type_data(!foo<>) -> () { // expected-error {{expected string literal data in dialect symbol}}
  return
}

// -----

func @dialect_type_missing_greater(!foo<"") -> () { // expected-error {{expected '>' in dialect symbol}}
  return
}

// -----

func @type_alias_unknown(!unknown_alias) -> () { // expected-error {{undefined symbol alias id 'unknown_alias'}}
  return
}

// -----

// expected-error @+1 {{type names with a '.' are reserved for dialect-defined names}}
!foo.bar = i32

// -----

!missing_eq_alias type i32 // expected-error {{expected '=' in type alias definition}}

// -----

!missing_kw_type_alias = i32 // expected-error {{expected 'type' in type alias definition}}

// -----

!missing_type_alias = type // expected-error@+2 {{expected non-function type}}

// -----

!redef_alias = type i32
!redef_alias = type i32 // expected-error {{redefinition of type alias id 'redef_alias'}}

// -----

// Check ill-formed opaque tensor.
func @complex_loops() {
  affine.for %i1 = 1 to 100 {
  // expected-error @+1 {{expected '"' in string literal}}
  "opaqueIntTensor"(){bar: opaque<"", tensor<2x1x4xi32>, "0x686]>} : () -> ()

// -----

func @mi() {
  // expected-error @+1 {{expected element literal of primitive type}}
  "fooi64"(){bar: sparse<vector<1xi64>,[,[,1]

// -----

func @invalid_tensor_literal() {
  // expected-error @+1 {{expected 1-d tensor for values}}
  "foof16"(){bar: sparse<vector<1x1x1xf16>, [[0, 0, 0]],  [[-2.0]]]>} : () -> ()

// -----

func @invalid_tensor_literal() {
  // expected-error @+1 {{expected element literal of primitive type}}
  "fooi16"(){bar: sparse<tensor<2x2x2xi16>, [[1, 1, 0], [0, 1, 0], [0,, [[0, 0, 0]], [-2.0]>} : () -> ()

// -----

func @invalid_affine_structure() {
  %c0 = constant 0 : index
  %idx = affine.apply (d0, d1) (%c0, %c0) // expected-error {{expected '->' or ':'}}
  return
}

// -----

func @missing_for_max(%arg0: index, %arg1: index, %arg2: memref<100xf32>) {
  // expected-error @+1 {{lower loop bound affine map with multiple results requires 'max' prefix}}
  affine.for %i0 = ()[s]->(0,s-1)()[%arg0] to %arg1 {
  }
  return
}

// -----

func @missing_for_min(%arg0: index, %arg1: index, %arg2: memref<100xf32>) {
  // expected-error @+1 {{upper loop bound affine map with multiple results requires 'min' prefix}}
  affine.for %i0 = %arg0 to ()[s]->(100,s+1)()[%arg1] {
  }
  return
}

// -----

// expected-error @+1 {{vector types must have positive constant sizes}}
func @zero_vector_type() -> vector<0xi32>

// -----

// expected-error @+1 {{vector types must have positive constant sizes}}
func @zero_in_vector_type() -> vector<1x0xi32>

// -----

// expected-error @+1 {{expected dimension size in vector type}}
func @negative_vector_size() -> vector<-1xi32>

// -----

// expected-error @+1 {{expected non-function type}}
func @negative_in_vector_size() -> vector<1x-1xi32>

// -----

// expected-error @+1 {{expected non-function type}}
func @negative_memref_size() -> memref<-1xi32>

// -----

// expected-error @+1 {{expected non-function type}}
func @negative_in_memref_size() -> memref<1x-1xi32>

// -----

// expected-error @+1 {{expected non-function type}}
func @negative_tensor_size() -> tensor<-1xi32>

// -----

// expected-error @+1 {{expected non-function type}}
func @negative_in_tensor_size() -> tensor<1x-1xi32>

// -----

func @invalid_nested_dominance() {
  "foo.region"() ({
    // expected-error @+1 {{operand #0 does not dominate this use}}
    "foo.use" (%1) : (i32) -> ()
    br ^bb2

  ^bb2:
    // expected-note @+1 {{operand defined here}}
    %1 = constant 0 : i32
    "foo.yield" () : () -> ()
  }) : () -> ()
  return
}

// -----

// expected-error @+1 {{unbalanced ']' character in pretty dialect name}}
func @invalid_unknown_type_dialect_name() -> !invalid.dialect<!x@#]!@#>

// -----

// expected-error @+1 {{@ identifier expected to start with letter or '_'}}
func @$invalid_function_name()

// -----

// expected-error @+1 {{function arguments may only have dialect attributes}}
func @invalid_func_arg_attr(i1 {non_dialect_attr: 10})

// -----

// expected-error @+1 {{expected '<' in tuple type}}
func @invalid_tuple_missing_less(tuple i32>)

// -----

// expected-error @+1 {{expected '>' in tuple type}}
func @invalid_tuple_missing_greater(tuple<i32)

// -----

// Should not crash because of deletion order here.
func @invalid_region_dominance() {
  "foo.use" (%1) : (i32) -> ()
  "foo.region"() ({
    %1 = constant 0 : i32  // This value is used outside of the region.
    "foo.yield" () : () -> ()
  }, {
    // expected-error @+1 {{expected operation name in quotes}}
    %2 = constant 1 i32  // Syntax error causes region deletion.
  }) : () -> ()
  return
}

// -----

// Should not crash because of deletion order here.
func @invalid_region_block() {
  "foo.branch"()[^bb2] : () -> ()  // Attempt to jump into the region.

^bb1:
  "foo.region"() ({
    ^bb2:
      "foo.yield"() : () -> ()
  }, {
    // expected-error @+1 {{expected operation name in quotes}}
    %2 = constant 1 i32  // Syntax error causes region deletion.
  }) : () -> ()
}

// -----

// Should not crash because of deletion order here.
func @invalid_region_dominance() {
  "foo.use" (%1) : (i32) -> ()
  "foo.region"() ({
    "foo.region"() ({
      %1 = constant 0 : i32  // This value is used outside of the region.
      "foo.yield" () : () -> ()
    }) : () -> ()
  }, {
    // expected-error @+1 {{expected operation name in quotes}}
    %2 = constant 1 i32  // Syntax error causes region deletion.
  }) : () -> ()
  return
}

// -----

func @unfinished_region_list() {
  // expected-error@+1 {{expected ')' to end region list}}
  "region"() ({},{},{} : () -> ()
}

// -----

func @multi_result_missing_count() {
  // expected-error@+1 {{expected integer number of results}}
  %0: = "foo" () : () -> (i32, i32)
  return
}

// -----

func @multi_result_zero_count() {
  // expected-error@+1 {{expected named operation to have atleast 1 result}}
  %0:0 = "foo" () : () -> (i32, i32)
  return
}

// -----

func @multi_result_invalid_identifier() {
  // expected-error@+1 {{expected valid ssa identifier}}
  %0, = "foo" () : () -> (i32, i32)
  return
}

// -----

func @multi_result_mismatch_count() {
  // expected-error@+1 {{operation defines 2 results but was provided 1 to bind}}
  %0:1 = "foo" () : () -> (i32, i32)
  return
}

// -----

func @multi_result_mismatch_count() {
  // expected-error@+1 {{operation defines 2 results but was provided 3 to bind}}
  %0, %1, %3 = "foo" () : () -> (i32, i32)
  return
}

// -----

func @no_result_with_name() {
  // expected-error@+1 {{cannot name an operation with no results}}
  %0 = "foo" () : () -> ()
  return
}

// -----

func @conflicting_names() {
  // expected-note@+1 {{previously defined here}}
  %foo, %bar  = "foo" () : () -> (i32, i32)

  // expected-error@+1 {{redefinition of SSA value '%bar'}}
  %bar, %baz  = "foo" () : () -> (i32, i32)
  return
}

// -----

func @ssa_name_missing_eq() {
  // expected-error@+1 {{expected '=' after SSA name}}
  %0:2 "foo" () : () -> (i32, i32)
  return
}

// -----

// expected-error @+1 {{invalid element type for complex}}
func @bad_complex(complex<memref<2x4xi8>>)

// -----

// expected-error @+1 {{expected '<' in complex type}}
func @bad_complex(complex memref<2x4xi8>>)

// -----

// expected-error @+1 {{expected '>' in complex type}}
func @bad_complex(complex<i32)

// -----

// expected-error @+1 {{attribute names with a '.' are reserved for dialect-defined names}}
#foo.attr = i32

// -----

func @invalid_region_dominance() {
  "foo.region"() ({
    // expected-error @+1 {{operand #0 does not dominate this use}}
    "foo.use" (%def) : (i32) -> ()
    "foo.yield" () : () -> ()
  }, {
    // expected-note @+1 {{operand defined here}}
    %def = "foo.def" () : () -> i32
  }) : () -> ()
  return
}
