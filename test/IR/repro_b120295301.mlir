// RUN: mlir-opt %s | FileCheck %s

cfgfunc @testType(tensor<1x224x224x3xf32>) -> tensor<96xf32> {
bb0(%arg0: tensor<1x224x224x3xf32>):
  %1  = "constant"() {value: splat<tensor<1xf32>, 0.1>} : () -> (tensor<1xf32>)
  %2  = "constant"() {value: splat<tensor<2xf32>, 0.1>} : () -> (tensor<2xf32>)
  %3  = "constant"() {value: splat<tensor<3xf32>, 0.1>} : () -> (tensor<3xf32>)
  %4  = "constant"() {value: splat<tensor<4xf32>, 0.1>} : () -> (tensor<4xf32>)
  %5  = "constant"() {value: splat<tensor<5xf32>, 0.1>} : () -> (tensor<5xf32>)
  %6  = "constant"() {value: splat<tensor<6xf32>, 0.1>} : () -> (tensor<6xf32>)
  %7  = "constant"() {value: splat<tensor<7xf32>, 0.1>} : () -> (tensor<7xf32>)
  %8  = "constant"() {value: splat<tensor<8xf32>, 0.1>} : () -> (tensor<8xf32>)
  %9  = "constant"() {value: splat<tensor<9xf32>, 0.1>} : () -> (tensor<9xf32>)
  %10  = "constant"() {value: splat<tensor<10xf32>, 0.1>} : () -> (tensor<10xf32>)
  %11  = "constant"() {value: splat<tensor<11xf32>, 0.1>} : () -> (tensor<11xf32>)
  %12  = "constant"() {value: splat<tensor<12xf32>, 0.1>} : () -> (tensor<12xf32>)
  %13  = "constant"() {value: splat<tensor<13xf32>, 0.1>} : () -> (tensor<13xf32>)
  %14  = "constant"() {value: splat<tensor<14xf32>, 0.1>} : () -> (tensor<14xf32>)
  %15  = "constant"() {value: splat<tensor<15xf32>, 0.1>} : () -> (tensor<15xf32>)
  %16  = "constant"() {value: splat<tensor<16xf32>, 0.1>} : () -> (tensor<16xf32>)
  %17  = "constant"() {value: splat<tensor<17xf32>, 0.1>} : () -> (tensor<17xf32>)
  %18  = "constant"() {value: splat<tensor<18xf32>, 0.1>} : () -> (tensor<18xf32>)
  %19  = "constant"() {value: splat<tensor<19xf32>, 0.1>} : () -> (tensor<19xf32>)
  %20  = "constant"() {value: splat<tensor<20xf32>, 0.1>} : () -> (tensor<20xf32>)
  %21  = "constant"() {value: splat<tensor<21xf32>, 0.1>} : () -> (tensor<21xf32>)
  %22  = "constant"() {value: splat<tensor<22xf32>, 0.1>} : () -> (tensor<22xf32>)
  %23  = "constant"() {value: splat<tensor<23xf32>, 0.1>} : () -> (tensor<23xf32>)
  %24  = "constant"() {value: splat<tensor<24xf32>, 0.1>} : () -> (tensor<24xf32>)
  %25  = "constant"() {value: splat<tensor<25xf32>, 0.1>} : () -> (tensor<25xf32>)
  %26  = "constant"() {value: splat<tensor<26xf32>, 0.1>} : () -> (tensor<26xf32>)
  %27  = "constant"() {value: splat<tensor<27xf32>, 0.1>} : () -> (tensor<27xf32>)
  %28  = "constant"() {value: splat<tensor<28xf32>, 0.1>} : () -> (tensor<28xf32>)
  %29  = "constant"() {value: splat<tensor<29xf32>, 0.1>} : () -> (tensor<29xf32>)
  %30  = "constant"() {value: splat<tensor<30xf32>, 0.1>} : () -> (tensor<30xf32>)
  %31  = "constant"() {value: splat<tensor<31xf32>, 0.1>} : () -> (tensor<31xf32>)
  %32  = "constant"() {value: splat<tensor<32xf32>, 0.1>} : () -> (tensor<32xf32>)
  %33  = "constant"() {value: splat<tensor<33xf32>, 0.1>} : () -> (tensor<33xf32>)
  %34  = "constant"() {value: splat<tensor<34xf32>, 0.1>} : () -> (tensor<34xf32>)
  %35  = "constant"() {value: splat<tensor<35xf32>, 0.1>} : () -> (tensor<35xf32>)
  %36  = "constant"() {value: splat<tensor<36xf32>, 0.1>} : () -> (tensor<36xf32>)
  %37  = "constant"() {value: splat<tensor<37xf32>, 0.1>} : () -> (tensor<37xf32>)
  %38  = "constant"() {value: splat<tensor<38xf32>, 0.1>} : () -> (tensor<38xf32>)
  %39  = "constant"() {value: splat<tensor<39xf32>, 0.1>} : () -> (tensor<39xf32>)
  %40  = "constant"() {value: splat<tensor<40xf32>, 0.1>} : () -> (tensor<40xf32>)
  %41  = "constant"() {value: splat<tensor<41xf32>, 0.1>} : () -> (tensor<41xf32>)
  %42  = "constant"() {value: splat<tensor<42xf32>, 0.1>} : () -> (tensor<42xf32>)
  %43  = "constant"() {value: splat<tensor<43xf32>, 0.1>} : () -> (tensor<43xf32>)
  %44  = "constant"() {value: splat<tensor<44xf32>, 0.1>} : () -> (tensor<44xf32>)
  %45  = "constant"() {value: splat<tensor<45xf32>, 0.1>} : () -> (tensor<45xf32>)
  %46  = "constant"() {value: splat<tensor<46xf32>, 0.1>} : () -> (tensor<46xf32>)
  %47  = "constant"() {value: splat<tensor<47xf32>, 0.1>} : () -> (tensor<47xf32>)
  %48  = "constant"() {value: splat<tensor<48xf32>, 0.1>} : () -> (tensor<48xf32>)
  %49  = "constant"() {value: splat<tensor<49xf32>, 0.1>} : () -> (tensor<49xf32>)
  %50  = "constant"() {value: splat<tensor<50xf32>, 0.1>} : () -> (tensor<50xf32>)
  %51  = "constant"() {value: splat<tensor<51xf32>, 0.1>} : () -> (tensor<51xf32>)
  %52  = "constant"() {value: splat<tensor<52xf32>, 0.1>} : () -> (tensor<52xf32>)
  %53  = "constant"() {value: splat<tensor<53xf32>, 0.1>} : () -> (tensor<53xf32>)
  %54  = "constant"() {value: splat<tensor<54xf32>, 0.1>} : () -> (tensor<54xf32>)
  %55  = "constant"() {value: splat<tensor<55xf32>, 0.1>} : () -> (tensor<55xf32>)
  %56  = "constant"() {value: splat<tensor<56xf32>, 0.1>} : () -> (tensor<56xf32>)
  %57  = "constant"() {value: splat<tensor<57xf32>, 0.1>} : () -> (tensor<57xf32>)
  %58  = "constant"() {value: splat<tensor<58xf32>, 0.1>} : () -> (tensor<58xf32>)
  %59  = "constant"() {value: splat<tensor<59xf32>, 0.1>} : () -> (tensor<59xf32>)
  %60  = "constant"() {value: splat<tensor<60xf32>, 0.1>} : () -> (tensor<60xf32>)
  %61  = "constant"() {value: splat<tensor<61xf32>, 0.1>} : () -> (tensor<61xf32>)
  %62  = "constant"() {value: splat<tensor<62xf32>, 0.1>} : () -> (tensor<62xf32>)
  %63  = "constant"() {value: splat<tensor<63xf32>, 0.1>} : () -> (tensor<63xf32>)
  %64  = "constant"() {value: splat<tensor<64xf32>, 0.1>} : () -> (tensor<64xf32>)
  %65  = "constant"() {value: splat<tensor<65xf32>, 0.1>} : () -> (tensor<65xf32>)
  %66  = "constant"() {value: splat<tensor<66xf32>, 0.1>} : () -> (tensor<66xf32>)
  %67  = "constant"() {value: splat<tensor<67xf32>, 0.1>} : () -> (tensor<67xf32>)
  %68  = "constant"() {value: splat<tensor<68xf32>, 0.1>} : () -> (tensor<68xf32>)
  %69  = "constant"() {value: splat<tensor<69xf32>, 0.1>} : () -> (tensor<69xf32>)
  %70  = "constant"() {value: splat<tensor<70xf32>, 0.1>} : () -> (tensor<70xf32>)
  %71  = "constant"() {value: splat<tensor<71xf32>, 0.1>} : () -> (tensor<71xf32>)
  %72  = "constant"() {value: splat<tensor<72xf32>, 0.1>} : () -> (tensor<72xf32>)
  %73  = "constant"() {value: splat<tensor<73xf32>, 0.1>} : () -> (tensor<73xf32>)
  %74  = "constant"() {value: splat<tensor<74xf32>, 0.1>} : () -> (tensor<74xf32>)
  %75  = "constant"() {value: splat<tensor<75xf32>, 0.1>} : () -> (tensor<75xf32>)
  %76  = "constant"() {value: splat<tensor<76xf32>, 0.1>} : () -> (tensor<76xf32>)
  %77  = "constant"() {value: splat<tensor<77xf32>, 0.1>} : () -> (tensor<77xf32>)
  %78  = "constant"() {value: splat<tensor<78xf32>, 0.1>} : () -> (tensor<78xf32>)
  %79  = "constant"() {value: splat<tensor<79xf32>, 0.1>} : () -> (tensor<79xf32>)
  %80  = "constant"() {value: splat<tensor<80xf32>, 0.1>} : () -> (tensor<80xf32>)
  %81  = "constant"() {value: splat<tensor<81xf32>, 0.1>} : () -> (tensor<81xf32>)
  %82  = "constant"() {value: splat<tensor<82xf32>, 0.1>} : () -> (tensor<82xf32>)
  %83  = "constant"() {value: splat<tensor<83xf32>, 0.1>} : () -> (tensor<83xf32>)
  %84  = "constant"() {value: splat<tensor<84xf32>, 0.1>} : () -> (tensor<84xf32>)
  %85  = "constant"() {value: splat<tensor<85xf32>, 0.1>} : () -> (tensor<85xf32>)
  %86  = "constant"() {value: splat<tensor<86xf32>, 0.1>} : () -> (tensor<86xf32>)
  %87  = "constant"() {value: splat<tensor<87xf32>, 0.1>} : () -> (tensor<87xf32>)
  %88  = "constant"() {value: splat<tensor<88xf32>, 0.1>} : () -> (tensor<88xf32>)
  %89  = "constant"() {value: splat<tensor<89xf32>, 0.1>} : () -> (tensor<89xf32>)
  %90  = "constant"() {value: splat<tensor<90xf32>, 0.1>} : () -> (tensor<90xf32>)
  %91  = "constant"() {value: splat<tensor<91xf32>, 0.1>} : () -> (tensor<91xf32>)
  %92  = "constant"() {value: splat<tensor<92xf32>, 0.1>} : () -> (tensor<92xf32>)
  %93  = "constant"() {value: splat<tensor<93xf32>, 0.1>} : () -> (tensor<93xf32>)
  %94  = "constant"() {value: splat<tensor<94xf32>, 0.1>} : () -> (tensor<94xf32>)
  %95  = "constant"() {value: splat<tensor<95xf32>, 0.1>} : () -> (tensor<95xf32>)
  %96  = "constant"() {value: splat<tensor<96xf32>, 0.1>} : () -> (tensor<96xf32>)
  %97  = "constant"() {value: splat<tensor<97xf32>, 0.1>} : () -> (tensor<97xf32>)
  %98  = "constant"() {value: splat<tensor<98xf32>, 0.1>} : () -> (tensor<98xf32>)
  %99  = "constant"() {value: splat<tensor<99xf32>, 0.1>} : () -> (tensor<99xf32>)
  %100  = "constant"() {value: splat<tensor<100xf32>, 0.1>} : () -> (tensor<100xf32>)
  %101  = "constant"() {value: splat<tensor<101xf32>, 0.1>} : () -> (tensor<101xf32>)
  %102  = "constant"() {value: splat<tensor<102xf32>, 0.1>} : () -> (tensor<102xf32>)
  return %96 : tensor<96xf32>
}
// CHECK: testType
// CHECK: return %cst_94
