/*
 * Copyright (C) 2009 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

extern "C" void dvmCompilerTemplateStart(void);

/*
 * Determine the initial instruction set to be used for this trace.
 * Later components may decide to change this.
 */
JitInstructionSetType dvmCompilerInstructionSet(void)
{
    return DALVIK_JIT_THUMB2;
}

/* First, declare dvmCompiler_TEMPLATE_XXX for each template */
#define JIT_TEMPLATE(X) extern "C" void dvmCompiler_TEMPLATE_##X();
#include "../../../template/armv5te-vfp/TemplateOpList.h"
#undef JIT_TEMPLATE

/* Architecture-specific initializations and checks go here */
bool dvmCompilerArchVariantInit(void)
{
    int i = 0;

    /*
     * Then, populate the templateEntryOffsets array with the offsets from the
     * the dvmCompilerTemplateStart symbol for each template.
     */
#define JIT_TEMPLATE(X) templateEntryOffsets[i++] = \
    (intptr_t) dvmCompiler_TEMPLATE_##X - (intptr_t) dvmCompilerTemplateStart;
#include "../../../template/armv5te-vfp/TemplateOpList.h"
#undef JIT_TEMPLATE

    /* Target-specific configuration */
    gDvmJit.jitTableSize = 1 << 12; // 4096
    gDvmJit.jitTableMask = gDvmJit.jitTableSize - 1;
    if (gDvmJit.threshold != 0) {
        gDvmJit.threshold = 40;
    }
    gDvmJit.codeCacheSize = 1024*1024;

#if defined(WITH_SELF_VERIFICATION)
    /* Force into blocking */
    gDvmJit.blockingMode = true;
    gDvm.nativeDebuggerActive = true;
#endif

    /* Codegen-specific assumptions */
    assert(OFFSETOF_MEMBER(ClassObject, vtable) < 128 &&
           (OFFSETOF_MEMBER(ClassObject, vtable) & 0x3) == 0);
    assert(OFFSETOF_MEMBER(ArrayObject, length) < 128 &&
           (OFFSETOF_MEMBER(ArrayObject, length) & 0x3) == 0);
    assert(OFFSETOF_MEMBER(ArrayObject, contents) < 256);

    /* Up to 5 args are pushed on top of FP - sizeofStackSaveArea */
    assert(sizeof(StackSaveArea) < 236);

    /*
     * EA is calculated by doing "Rn + imm5 << 2". Make sure that the last
     * offset from the struct is less than 128.
     */
    if ((offsetof(Thread, jitToInterpEntries) +
         sizeof(struct JitToInterpEntries)) >= 128) {
        ALOGE("Thread.jitToInterpEntries size overflow");
        dvmAbort();
    }

    /* FIXME - comment out the following to enable method-based JIT */
    gDvmJit.disableOpt |= (1 << kMethodJit);

    // Make sure all threads have current values
    dvmJitUpdateThreadStateAll();

    return true;
}

int dvmCompilerTargetOptHint(int key)
{
    int res;
    switch (key) {
        case kMaxHoistDistance:
            res = 7;
            break;
        default:
            ALOGE("Unknown target optimization hint key: %d",key);
            res = 0;
    }
    return res;
}

void dvmCompilerGenMemBarrier(CompilationUnit *cUnit, int barrierKind)
{
#if ANDROID_SMP != 0
    ArmLIR *dmb = newLIR1(cUnit, kThumb2Dmb, barrierKind);
    dmb->defMask = ENCODE_ALL;
#endif
}
