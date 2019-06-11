/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file RamTransformer.cpp
 *
 * Defines the interface for RAM transformation passes.
 *
 ***********************************************************************/

#include "RamTransformer.h"
#include "RamTranslationUnit.h"

namespace souffle {

bool RamTransformer::apply(RamTranslationUnit& translationUnit) {
    bool changed = transform(translationUnit);
    if (changed) {
        translationUnit.invalidateAnalyses();
        std::stringstream ramProgStr;
        ramProgStr << *translationUnit.getProgram();
        translationUnit.getDebugReport().addSection(
                DebugReporter::getCodeSection(getName(), "RAM Program after " + getName(), ramProgStr.str()));

    } else {
        translationUnit.getDebugReport().addSection(
                DebugReportSection(getName(), "After " + getName() + " " + " (unchanged)", {}, ""));
    }
    /* Abort evaluation of the program if errors were encountered */
    if (translationUnit.getErrorReport().getNumErrors() != 0) {
        std::cerr << translationUnit.getErrorReport();
        std::cerr << std::to_string(translationUnit.getErrorReport().getNumErrors()) +
                             " errors generated, evaluation aborted"
                  << std::endl;
        exit(1);
    }
    return changed;
}

}  // end of namespace souffle
