#ifndef V8_TORQUE_BASE_FROM_DSL_BASE_H__
#define V8_TORQUE_BASE_FROM_DSL_BASE_H__

#include "src/code-stub-assembler.h"

 namespace v8 {
namespace internal {

class BaseBuiltinsFromDSLAssembler: public CodeStubAssembler {
 public:
  explicit BaseBuiltinsFromDSLAssembler(compiler::CodeAssemblerState* state) : CodeStubAssembler(state) {}

  using Node = compiler::Node;
  template <class T>
  using TNode = compiler::TNode<T>;
  template <class T>
  using SloppyTNode = compiler::SloppyTNode<T>;

  TNode<Oddball> Hole();
  TNode<Oddball> Null();
  TNode<Oddball> Undefined();
  TNode<Oddball> True();
  TNode<Oddball> False();
  TNode<String> kLengthString();
  TNode<BoolT> ElementsKindNotEqual(TNode<Int32T> p_k1, TNode<Int32T> p_k2);
  TNode<Number> Min(TNode<Number> p_x, TNode<Number> p_y);
  TNode<Number> Max(TNode<Number> p_x, TNode<Number> p_y);
  TNode<HeapObject> CastHeapObject12ATHeapObject(TNode<HeapObject> p_o, Label* label_CastError_0);
  TNode<FixedArray> CastHeapObject12ATFixedArray(TNode<HeapObject> p_o, Label* label_CastError_1);
  TNode<FixedDoubleArray> CastHeapObject18ATFixedDoubleArray(TNode<HeapObject> p_o, Label* label_CastError_2);
  TNode<JSDataView> CastHeapObject12ATJSDataView(TNode<HeapObject> p_o, Label* label_CastError_3);
  TNode<JSReceiver> CastHeapObject45UT17ATJSBoundFunction12ATJSFunction9ATJSProxy(TNode<HeapObject> p_o, Label* label_CastError_4);
  TNode<JSArray> CastHeapObject9ATJSArray(TNode<HeapObject> p_o, Label* label_CastError_5);
  TNode<Smi> Cast5ATSmi(TNode<Object> p_o, Label* label_CastError_6);
  TNode<Number> Cast22UT12ATHeapNumber5ATSmi(TNode<Object> p_o, Label* label_CastError_7);
  TNode<IntPtrT> FromConstexpr8ATintptr(int31_t p_i);
  TNode<Int32T> FromConstexpr7ATint31(int31_t p_i);
  TNode<Int32T> FromConstexpr7ATint32(int31_t p_i);
  TNode<Uint32T> FromConstexpr8ATuint32(int31_t p_i);
  TNode<UintPtrT> FromConstexpr9ATuintptr(int31_t p_i);
  TNode<Smi> FromConstexpr5ATSmi(int31_t p_i);
  TNode<Number> FromConstexpr22UT12ATHeapNumber5ATSmi(int31_t p_i);
  TNode<Float64T> FromConstexpr9ATfloat64(int31_t p_i);
  TNode<IntPtrT> FromConstexpr8ATintptr(int32_t p_i);
  TNode<Int32T> FromConstexpr7ATint32(int32_t p_i);
  TNode<Number> FromConstexpr22UT12ATHeapNumber5ATSmi(int32_t p_i);
  TNode<Number> FromConstexpr22UT12ATHeapNumber5ATSmi(double p_f);
  TNode<BoolT> FromConstexpr6ATbool(bool p_b);
  TNode<Smi> FromConstexpr14ATLanguageMode(LanguageMode p_b);
  TNode<Int32T> FromConstexpr14ATElementsKind(ElementsKind p_e);
  TNode<String> FromConstexpr8ATString(const char* p_s);
  TNode<Object> FromConstexpr22UT12ATHeapObject5ATSmi(const char* p_s);
  TNode<Int32T> Convert7ATint32(TNode<Int32T> p_elementsKind);
  TNode<Number> Convert22UT12ATHeapNumber5ATSmi(TNode<Int32T> p_i);
  TNode<IntPtrT> Convert8ATintptr(TNode<Int32T> p_i);
  TNode<Smi> Convert5ATSmi(TNode<Int32T> p_i);
  TNode<Number> Convert22UT12ATHeapNumber5ATSmi(TNode<Uint32T> p_ui);
  TNode<Smi> Convert5ATSmi(TNode<Uint32T> p_ui);
  TNode<UintPtrT> Convert9ATuintptr(TNode<Uint32T> p_ui);
  TNode<Int32T> Convert7ATint32(TNode<IntPtrT> p_i);
  TNode<Smi> Convert5ATSmi(TNode<IntPtrT> p_i);
  TNode<Uint32T> Convert8ATuint32(TNode<UintPtrT> p_ui);
  TNode<IntPtrT> Convert8ATintptr(TNode<Smi> p_s);
  TNode<Int32T> Convert7ATint32(TNode<Smi> p_s);
  TNode<Float64T> Convert9ATfloat64(TNode<HeapNumber> p_h);
  TNode<Float64T> Convert9ATfloat64(TNode<Number> p_n);
  TNode<Float64T> Convert9ATfloat64(TNode<Float32T> p_f);
  TNode<Number> Convert22UT12ATHeapNumber5ATSmi(TNode<Float64T> p_d);
  TNode<Float64T> Convert9ATfloat64(TNode<UintPtrT> p_ui);
  TNode<Number> Convert22UT12ATHeapNumber5ATSmi(TNode<UintPtrT> p_ui);
  TNode<UintPtrT> Convert9ATuintptr(TNode<Float64T> p_d);
  TNode<UintPtrT> Convert9ATuintptr(TNode<RawPtrT> p_r);
  TNode<IntPtrT> Convert8ATintptr(TNode<RawPtrT> p_r);
  TNode<HeapNumber> UnsafeCast12ATHeapNumber(TNode<Number> p_n);
  TNode<Object> UnsafeCast22UT12ATHeapObject5ATSmi(TNode<Object> p_o);
  TNode<FixedArray> UnsafeCast12ATFixedArray(TNode<Object> p_o);
  TNode<FixedDoubleArray> UnsafeCast18ATFixedDoubleArray(TNode<Object> p_o);
  TNode<HeapNumber> UnsafeCast12ATHeapNumber(TNode<Object> p_o);
  TNode<JSReceiver> UnsafeCast45UT17ATJSBoundFunction12ATJSFunction9ATJSProxy(TNode<Object> p_o);
  TNode<Smi> UnsafeCast5ATSmi(TNode<Object> p_o);
  TNode<Number> UnsafeCast22UT12ATHeapNumber5ATSmi(TNode<Object> p_o);
  TNode<HeapObject> UnsafeCast12ATHeapObject(TNode<Object> p_o);
  TNode<JSArray> UnsafeCast9ATJSArray(TNode<Object> p_o);
  TNode<FixedTypedArrayBase> UnsafeCast21ATFixedTypedArrayBase(TNode<Object> p_o);
  TNode<NumberDictionary> UnsafeCast18ATNumberDictionary(TNode<Object> p_o);
  TNode<JSReceiver> UnsafeCast12ATJSReceiver(TNode<Object> p_o);
  TNode<JSObject> UnsafeCast10ATJSObject(TNode<Object> p_o);
  TNode<Map> UnsafeCast5ATMap(TNode<Object> p_o);
  TNode<FixedArrayBase> UnsafeCast16ATFixedArrayBase(TNode<Object> p_o);
  TNode<Map> kCOWMap();
  TNode<FixedArrayBase> kEmptyFixedArray();
  void EnsureFastJSArray(TNode<Context> p_context, TNode<Object> p_object, Label* label_Bailout_8);
  void StoreFixedDoubleArrayNumber(TNode<FixedDoubleArray> p_a, TNode<Smi> p_index, TNode<Number> p_value);
  void StoreFixedDoubleArrayElementWithSmiIndex(TNode<FixedDoubleArray> p_array, TNode<Smi> p_index, TNode<Float64T> p_value);
  TNode<Int32T> AllowDoubleElements(TNode<Int32T> p_kind);
  TNode<Int32T> AllowNonNumberElements(TNode<Int32T> p_kind);
  TNode<Object> LoadElementNoHole12ATFixedArray(TNode<JSArray> p_a, TNode<Smi> p_index, Label* label_IfHole_23);
  TNode<Object> LoadElementNoHole18ATFixedDoubleArray(TNode<JSArray> p_a, TNode<Smi> p_index, Label* label_IfHole_27);
  TNode<BoolT> NumberIsNaN(TNode<Number> p_number);
  TNode<BoolT> ToBoolean(TNode<Object> p_obj);
  TNode<Number> ToIndex(TNode<Object> p_input, TNode<Context> p_context, Label* label_RangeError_32);
  TNode<Code> UnsafeCast108FT9ATContext22UT12ATHeapObject5ATSmi22UT12ATHeapObject5ATSmi22UT12ATHeapObject5ATSmi22UT12ATHeapNumber5ATSmi(TNode<Object> p_o);
  TNode<Code> UnsafeCast70FT9ATContext12ATFixedArray12ATHeapObject5ATSmi22UT12ATHeapObject5ATSmi(TNode<Object> p_o);
  TNode<Code> UnsafeCast76FT9ATContext12ATFixedArray12ATHeapObject5ATSmi22UT12ATHeapObject5ATSmi5ATSmi(TNode<Object> p_o);
  TNode<Code> UnsafeCast84FT9ATContext12ATJSReceiver22UT12ATHeapObject5ATSmi22UT12ATHeapNumber5ATSmi9ATBoolean(TNode<Object> p_o);
  TNode<FixedArray> Cast12ATFixedArray(TNode<HeapObject> p_o, Label* label_CastError_1276);
  TNode<FixedDoubleArray> Cast18ATFixedDoubleArray(TNode<HeapObject> p_o, Label* label_CastError_1278);
  TNode<JSArray> Cast9ATJSArray(TNode<Object> p_o, Label* label_CastError_1282);
  TNode<JSReceiver> Cast45UT17ATJSBoundFunction12ATJSFunction9ATJSProxy(TNode<Object> p_o, Label* label_CastError_1284);
  TNode<JSArray> Cast9ATJSArray(TNode<HeapObject> p_o, Label* label_CastError_1281);
  TNode<JSDataView> Cast12ATJSDataView(TNode<Object> p_o, Label* label_CastError_1361);
  TNode<Float64T> Convert9ATfloat64(int31_t p_i);
  TNode<IntPtrT> Convert8ATintptr(int31_t p_i);
  TNode<FixedArray> Cast12ATFixedArray(TNode<Object> p_o, Label* label_CastError_1277);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_TORQUE_BASE_FROM_DSL_BASE_H__