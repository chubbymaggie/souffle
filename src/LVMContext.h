/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2019, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file LVMContext.h
 *
 * Defines LVM interpreter context
 *
 ***********************************************************************/

#pragma once

#include "RamTypes.h"
#include <cassert>
#include <memory>
#include <vector>

namespace souffle {

/**
 * Evaluation context for Interpreter operations
 */
class LVMContext {
    std::vector<const RamDomain*> data;
    std::vector<RamDomain>* returnValues = nullptr;
    std::vector<bool>* returnErrors = nullptr;
    const std::vector<RamDomain>* args = nullptr;
    std::vector<std::unique_ptr<RamDomain[]>> allocatedDataContainer;

public:
    LVMContext(size_t size = 0) : data(size) {}
    virtual ~LVMContext() = default;

    const RamDomain*& operator[](size_t index) {
        if (index >= data.size()) {
            data.resize((index + 1));
        }
        return data[index];
    }

    const RamDomain* const& operator[](size_t index) const {
        return data[index];
    }

    /** Allocate a tuple.
     *  LVMContext has ownership of allocated tuples. */
    RamDomain* allocateNewTuple(size_t size) {
        std::unique_ptr<RamDomain[]> newTuple(new RamDomain[size]);
        allocatedDataContainer.push_back(std::move(newTuple));

        // Return the reference as raw pointer.
        return allocatedDataContainer.back().get();
    }

    std::vector<RamDomain>& getReturnValues() const {
        return *returnValues;
    }

    void setReturnValues(std::vector<RamDomain>& retVals) {
        returnValues = &retVals;
    }

    void addReturnValue(RamDomain val, bool err = false) {
        assert(returnValues != nullptr && returnErrors != nullptr);
        returnValues->push_back(val);
        returnErrors->push_back(err);
    }

    std::vector<bool>& getReturnErrors() const {
        return *returnErrors;
    }

    void setReturnErrors(std::vector<bool>& retErrs) {
        returnErrors = &retErrs;
    }

    const std::vector<RamDomain>& getArguments() const {
        return *args;
    }

    void setArguments(const std::vector<RamDomain>& a) {
        args = &a;
    }

    RamDomain getArgument(size_t i) const {
        assert(args != nullptr && i < args->size() && "argument out of range");
        return (*args)[i];
    }
};

}  // end of namespace souffle
