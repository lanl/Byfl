/*
 * Instrument code to keep track of run-time behavior:
 * module pass for maintaining function IDs
 *
 * By Rob Aulwes <rta@lanl.gov>
 */

#include "BytesMP.h"
#include "helpers.h"

namespace bytesflops_pass
{

    char BytesMP::ID = 1;

    BytesMP::~BytesMP()
    {
        // TODO Auto-generated destructor stub
    }

    bool BytesMP::runOnModule(Module & module)
    {

        return false;

        const BytesFlops & bf = getAnalysis<BytesFlops>();

        const std::map<std::string, KeyType_t> &
        func_key_map = bf.getFuncKeyMap();

        printf("func_key_map size = %ld...\n", func_key_map.size());

        // construct C array of keys, where the first element
        // is always the # of keys.
        LLVMContext & ctx = module.getContext();
        ArrayType* ArrayTy_0 = ArrayType::get(IntegerType::get(ctx, 64), func_key_map.size());

        // set up array pointers for array of strings
        PointerType *
        PointerTy = PointerType::get(IntegerType::get(ctx, 8), 0);
        ArrayType * ArrayPtrTy = ArrayType::get(PointerTy, func_key_map.size());

        // declare global array of keys
        GlobalVariable* gvar_array_key = new GlobalVariable(/*Module=*/module,
        /*Type=*/ArrayTy_0,
        /*isConstant=*/true,
        /*Linkage=*/GlobalValue::AppendingLinkage,
        /*Initializer=*/0, // has initializer, specified below
        /*Name=*/"bf_keys");
        gvar_array_key->setAlignment(16);

        // declare global array of function names matching keys
        // since the first item of the key array is the # of keys, then
        // the key of function bf_fname[i] is bf_key[i+1].
        GlobalVariable* gvar_array_names = new GlobalVariable(/*Module=*/module,
        /*Type=*/ArrayPtrTy,
        /*isConstant=*/true,
        /*Linkage=*/GlobalValue::AppendingLinkage,
        /*Initializer=*/0, // has initializer, specified below
        /*Name=*/"bf_fnames");
        gvar_array_names->setAlignment(16);

        ConstantInt *
        const_int32 = ConstantInt::get(ctx, APInt(32, StringRef("0"), 10));

        std::vector<Constant*> const_key_elems;
        std::vector<Constant*> const_fname_elems;

        // record # of keys first
        const_key_elems.push_back( ConstantInt::get(ctx, APInt(64, func_key_map.size())) );

        int i = 0;
        for ( auto it = func_key_map.begin(); it != func_key_map.end(); it++ )
        {
            const std::string & name = it->first;
            auto key = it->second;

            ArrayType *
            ArrayStrTy = ArrayType::get(IntegerType::get(ctx, 8), name.size() + 1);

            // record the key
            ConstantInt* const_int64 = ConstantInt::get(ctx, APInt(64, key));
            const_key_elems.push_back(const_int64);

            // record the name
            std::string gv_name = ".str" + std::to_string(i++);
            GlobalVariable* gvar_array_str = new GlobalVariable(/*Module=*/module,
            /*Type=*/ArrayStrTy,
            /*isConstant=*/true,
            /*Linkage=*/GlobalValue::PrivateLinkage,
            /*Initializer=*/0, // has initializer, specified below
            /*Name=*/gv_name.c_str());
            gvar_array_str->setAlignment(1);
            mark_as_used(module, gvar_array_str);

            std::vector<Constant*> const_ptr_indices;
            Constant *
            const_fname = ConstantDataArray::getString(ctx, name.c_str(), true);
            const_ptr_indices.push_back(const_int32);
            const_ptr_indices.push_back(const_int32);
            Constant *
            const_ptr = ConstantExpr::getGetElementPtr(gvar_array_str, const_ptr_indices);

            const_fname_elems.push_back(const_ptr);

            gvar_array_str->setInitializer(const_fname);
        }
        printf("# keys = %ld\n", ArrayTy_0->getNumElements());

        Constant* const_array_keys = ConstantArray::get(ArrayTy_0, const_key_elems);
        Constant* const_array_fnames = ConstantArray::get(ArrayPtrTy, const_fname_elems);

        // Global Variable Definitions
        gvar_array_key->setInitializer(const_array_keys);
        gvar_array_names->setInitializer(const_array_fnames);
        mark_as_used(module, gvar_array_key);
        mark_as_used(module, gvar_array_names);

        return true;
    }

//    static RegisterPass<BytesMP> H("bytesmp", "Bytes:flops module instrumentation");

} /* namespace bytesflops_pass */
