/**
 * Copyright (c) 2015, Jozef Stefan Institute, Quintelligence d.o.o. and contributors
 * All rights reserved.
 *
 * This source code is licensed under the FreeBSD license found in the
 * LICENSE file in the root directory of this source tree.
 */
#include "dmoz.h"

using namespace v8;

void TJsDmoz::Init(v8::Handle<v8::Object> Exports) {
    v8::Isolate* Isolate = v8::Isolate::GetCurrent();
    v8::HandleScope HandleScope(Isolate);

    // Add all methods, getters and setters here.
    NODE_SET_METHOD(Exports, "construct", _construct);
    NODE_SET_METHOD(Exports, "init", _initDmoz);
    NODE_SET_METHOD(Exports, "load", _loadDmoz);
    NODE_SET_METHOD(Exports, "classify", _classify);
}

PDMozCfy TJsDmoz::DMozCfy = NULL;

void TJsDmoz::construct(const v8::FunctionCallbackInfo<v8::Value>& Args) {
    v8::Isolate* Isolate = v8::Isolate::GetCurrent();
    v8::HandleScope HandleScope(Isolate);

    // parse out parameters
    const PJsonVal ParamJson = TNodeJsUtil::GetArgJson(Args, 0);
    const TStr RdfPath = ParamJson->GetObjStr("rdfPath");
    const TStr BowFNm = ParamJson->GetObjStr("bow");
    const TStr BowPartFNm = ParamJson->GetObjStr("bowPart");
    const TInt MnCatDocs = ParamJson->GetObjInt("categoryMinSize", 100);

    printf("using path: %s", RdfPath.CStr());

    printf("creating supporting structures ...\n");
    const PStemmer Stemmer = TStemmer::New(stmtPorter, true);
    const PSwSet SwSet = TSwSet::New(swstEn523);
    const PBowSim BowSim = TBowSim::New(bstCos);
    /* const PBowSim BowSim = TBowSim::New(bstCos); */
    const TStr RootCatNm = "Top";
    TStrV PosCatNmV; TStrV NegCatNmV;
    PNGramBs NGramBs;

    printf("creating DMoz base ...\n");
    const PDMozBs DMozBs = TDMozBs::LoadTxt(RdfPath, false, false);

    printf("total external URLs: %ld\n", DMozBs->GetExtUrls());

    printf("retrieve title&desc-string-vector\n");
    TStrV DocNmV;   // contains external URL ids
    TStrV DocStrV;
    DMozBs->GetSubTreeDocV(RootCatNm, PosCatNmV, NegCatNmV, DocNmV, DocStrV, false, 1);

    printf("creating BoW, total documents: %d\n", DocNmV.Len());
    PBowDocBs BowDocBs = TBowDocBs::New(SwSet, Stemmer, NGramBs);
    for (int DocN = 0; DocN < DocNmV.Len(); ++DocN) {
        const TStr& DocNm = DocNmV[DocN];
        if (DocN % 1000 == 0) {
            printf("document %d of %d\r", (DocN+1), DocNmV.Len());
        }

        const int DId = BowDocBs->AddHtmlDoc(DocNm, TStrV(), DocStrV[DocN]);
        const TStr UrlStr = DMozBs->GetExtUrlStr(DocNm.GetInt());
        BowDocBs->PutDocDescStr(DId, UrlStr);
    }
    BowDocBs->AssertOk();
    DocNmV.Clr();
    DocStrV.Clr();

    printf("saving BoW file\n");
    BowDocBs->SaveBin(BowFNm);

    printf("creating BoW partition\n");
    const PBowDocWgtBs BowDocWgtBs = TBowDocWgtBs::New(BowDocBs, bwwtLogDFNrmTFIDF);
    PBowDocPart BowDocPart=
           DMozBs->GetBowDocPart(RootCatNm, PosCatNmV, NegCatNmV, BowDocBs, BowDocWgtBs, BowSim, MnCatDocs);

    printf("saving BoW partition\n");
    BowDocPart->SaveBin(BowPartFNm);

    printf("done!\n");
}

void TJsDmoz::initDmoz(const v8::FunctionCallbackInfo<v8::Value>& Args) {
    v8::Isolate* Isolate = v8::Isolate::GetCurrent();
    v8::HandleScope HandleScope(Isolate);

    // parse out parameters
    TStr BowFNm = TNodeJsUtil::GetArgStr(Args, 0, "bow", "");
    TStr BowPartFNm = TNodeJsUtil::GetArgStr(Args, 0, "bowPart", "");
    TStr FilterFNm = TNodeJsUtil::GetArgStr(Args, 0, "filter", "");
    TStr OutFNm = TNodeJsUtil::GetArgStr(Args, 0, "classifier", "");
    // check we got all the necessary values
    EAssert(!BowFNm.Empty()); EAssert(!BowPartFNm.Empty());
    EAssert(!FilterFNm.Empty()); EAssert(!OutFNm.Empty());
    // create classifier
    printf("Loading BowDocBs ...\n");
    PBowDocBs BowDocBs = TBowDocBs::LoadBin(BowFNm);
    printf("Loading BowDocPart ...\n");
    PBowDocPart BowDocPart = TBowDocPart::LoadBin(BowPartFNm);
    printf("Creating classifier ...\n");
    DMozCfy = TDMozCfy::New(BowDocBs, BowDocPart, FilterFNm);
    printf("Saving classifier ...\n");
    TFOut FOut(OutFNm); DMozCfy->Save(FOut);
    printf("Done!\n");
}

void TJsDmoz::loadDmoz(const v8::FunctionCallbackInfo<v8::Value>& Args) {
    v8::Isolate* Isolate = v8::Isolate::GetCurrent();
    v8::HandleScope HandleScope(Isolate);

    // parse out parameters
    TStr CfyFNm = TNodeJsUtil::GetArgStr(Args, 0, "classifier", "");
    // check we got all the necessary values
    EAssert(!CfyFNm.Empty());
    // create classifier
    printf("Loading classifier ...\n");
    TFIn FIn(CfyFNm); DMozCfy = TDMozCfy::Load(FIn);
    printf("Done!\n");
}

void TJsDmoz::classify(const v8::FunctionCallbackInfo<v8::Value>& Args) {
    v8::Isolate* Isolate = v8::Isolate::GetCurrent();
    v8::HandleScope HandleScope(Isolate);

    // parse out parameters
    TStr Str = TNodeJsUtil::GetArgStr(Args, 0);
    const int MxCats = TNodeJsUtil::GetArgInt32(Args, 1, 3);
    // make sure we have classifier loaded
    EAssert(!DMozCfy.Empty());
    // classify
    TStrFltKdV CatNmWgtV, KeyWdWgtV;
    DMozCfy->Classify(Str, CatNmWgtV, KeyWdWgtV, MxCats);
    // translate to json
    v8::Local<v8::Object> ResObj = v8::Object::New(Isolate);
    v8::Local<v8::Object> CatArr = v8::Array::New(Isolate, CatNmWgtV.Len());
    for (int CatNmN = 0; CatNmN < CatNmWgtV.Len(); CatNmN++) {
        v8::Local<v8::Object> CatObj = v8::Object::New(Isolate);
        CatObj->Set(v8::String::NewFromUtf8(Isolate, "category"), v8::String::NewFromUtf8(Isolate, CatNmWgtV[CatNmN].Key.CStr()));
        CatObj->Set(v8::String::NewFromUtf8(Isolate, "weight"), v8::Number::New(Isolate, CatNmWgtV[CatNmN].Dat));
        CatArr->Set(CatNmN, CatObj);
    }
    ResObj->Set(v8::String::NewFromUtf8(Isolate, "categories"), CatArr);
    v8::Local<v8::Object> KeyWdArr = v8::Array::New(Isolate, KeyWdWgtV.Len());
    for (int KeyWdN = 0; KeyWdN < KeyWdWgtV.Len(); KeyWdN++) {
        KeyWdArr->Set(KeyWdN, v8::String::NewFromUtf8(Isolate, KeyWdWgtV[KeyWdN].Key.CStr()));
    }
    ResObj->Set(v8::String::NewFromUtf8(Isolate, "keywords"), KeyWdArr);
    Args.GetReturnValue().Set(ResObj);
}

void Init(Handle<Object> Exports) {
    TJsDmoz::Init(Exports);
}

NODE_MODULE(dmoz, Init);
