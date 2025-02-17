/****************************************************************************
  FileName     [ sat.cpp ]
  PackageName  [ sat ]
  Synopsis     [ Define miniSat solver interface functions ]
  Author       [ Chung-Yang (Ric) Huang, Cheng-Yin Wu ]
  Copyright    [ Copyleft(c) 2010-2014 LaDs(III), GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef SAT_C
#define SAT_C

#include "gvSat.h"
#include <cmath>
#include <iomanip>

GVSatSolver::GVSatSolver(GVNtkMgr* ntk) : _ntk(ntk) {
    _solver        = new SolverV();
    _solver->proof = new Proof();
    _assump.clear();
    _curVar = 0;
    _solver->newVar();
    ++_curVar;
    _ntkData = new vector<Var>[ntk->getNetSize()];
    for (uint32_t i = 0; i < ntk->getNetSize(); ++i) _ntkData[i].clear();
}

GVSatSolver::~GVSatSolver() {
    delete _solver;
    assumeRelease();
    for (uint32_t i = 0; i < _ntk->getNetSize(); ++i) _ntkData[i].clear();
    delete[] _ntkData;
}

void
GVSatSolver::reset() {
    delete _solver;
    _solver        = new SolverV();
    _solver->proof = new Proof();
    _assump.clear();
    _curVar = 0;
    _solver->newVar();
    ++_curVar;
    _ntkData = new vector<Var>[_ntk->getNetSize()];
    for (uint32_t i = 0; i < _ntk->getNetSize(); ++i) _ntkData[i].clear();
}

void
GVSatSolver::assumeRelease() {
    _assump.clear();
}

void
GVSatSolver::assumeProperty(const size_t& var, const bool& invert) {
    _assump.push(mkLit(getOriVar(var), invert ^ isNegFormula(var)));
}

void
GVSatSolver::assertProperty(const size_t& var, const bool& invert) {
    _solver->addUnit(mkLit(getOriVar(var), invert ^ isNegFormula(var)));
}

void
GVSatSolver::assumeProperty(const GVNetId& id, const bool& invert, const uint32_t& depth) {
    const Var var = getVerifyData(id, depth);
    _assump.push(mkLit(var, invert ^ id.cp));
}

void
GVSatSolver::assertProperty(const GVNetId& id, const bool& invert, const uint32_t& depth) {
    const Var var = getVerifyData(id, depth);
    _solver->addUnit(mkLit(var, invert ^ id.cp));
}

const bool
GVSatSolver::simplify() {
    return _solver->simplifyDB();
}

const bool
GVSatSolver::solve() {
    _solver->solve();
    return _solver->okay();
}

const bool
GVSatSolver::assump_solve() {
    bool result = _solver->solve(_assump);
    return result;
}

const GVBitVecX
GVSatSolver::getDataValue(const GVNetId& id, const uint32_t& depth) const {
    Var       var = getVerifyData(id, depth);
    uint32_t  i, width = 1;
    GVBitVecX value(width);
    if (isGVNetInverted(id)) {
        for (i = 0; i < width; ++i)
            if (gv_l_True == _solver->model[var + i]) value.set0(i);
            else value.set1(i);
    } else {
        for (i = 0; i < width; ++i)
            if (gv_l_True == _solver->model[var + i]) value.set1(i);
            else value.set0(i);
    }
    return value;
}

const bool
GVSatSolver::getDataValue(const size_t& var) const {
    return (isNegFormula(var)) ^ (gv_l_True == _solver->model[getOriVar(var)]);
}

const size_t
GVSatSolver::getFormula(const GVNetId& id, const uint32_t& depth) {
    Var var = getVerifyData(id, depth);
    return (id.fanin0Cp ? getNegVar(var) : getPosVar(var));
}

void
GVSatSolver::resizeNtkData(const uint32_t& num) {
    vector<Var>* tmp = new vector<Var>[_ntk->getNetSize()];
    for (uint32_t i = 0, j = (_ntk->getNetSize() - num); i < j; ++i) tmp[i] = _ntkData[i];
    delete[] _ntkData;
    _ntkData = tmp;
}

const Var
GVSatSolver::newVar() {
    Var cur_var = _curVar;
    _solver->newVar();
    _curVar++;
    return cur_var;
}

const Var
GVSatSolver::getVerifyData(const GVNetId& id, const uint32_t& depth) const {
    if (depth >= _ntkData[getGVNetIndex(id)].size()) return 0;
    else return _ntkData[getGVNetIndex(id)][depth];
}

void
GVSatSolver::add_FALSE_Formula(const GVNetId& out, const uint32_t& depth) {
    const uint32_t index = getGVNetIndex(out);
    _ntkData[index].push_back(newVar());
    _solver->addUnit(mkLit(_ntkData[index].back(), true));
}

void
GVSatSolver::add_PI_Formula(const GVNetId& out, const uint32_t& depth) {
    const uint32_t index = getGVNetIndex(out);
    _ntkData[index].push_back(newVar());
}

void
GVSatSolver::add_FF_Formula(const GVNetId& out, const uint32_t& depth) {
    const uint32_t index = getGVNetIndex(out);
    //  assert(depth == _ntkData[index].size());

    if (depth) {
        // Build FF I/O Relation
        const GVNetId in1  = _ntk->getInputNetId(out, 0);
        const Var     var1 = getVerifyData(in1, depth - 1);

        if (out.fanin0Cp) {
            // a <-> b
            _ntkData[index].push_back(newVar());
            Lit      a = mkLit(_ntkData[index].back());
            Lit      b = mkLit(var1, true);
            vec<Lit> lits;
            lits.clear();
            lits.push(~a);
            lits.push(b);
            _solver->addClause(lits);
            lits.clear();
            lits.push(a);
            lits.push(~b);
            _solver->addClause(lits);
            lits.clear();
        } else _ntkData[index].push_back(var1);
    } else { // Timeframe 0
        _ntkData[index].push_back(newVar());
    }
}

void
GVSatSolver::add_AND_Formula(const GVNetId& out, const uint32_t& depth) {
    const uint32_t index = getGVNetIndex(out);
    _ntkData[index].push_back(newVar());

    const Var&    var = _ntkData[index].back();
    // Build AND I/O Relation
    const GVNetId in1  = _ntk->getInputNetId(out, 0);
    const GVNetId in2  = _ntk->getInputNetId(out, 1);
    const Var     var1 = getVerifyData(in1, depth);
    const Var     var2 = getVerifyData(in2, depth);

    Lit y = mkLit(var);
    Lit a = mkLit(var1, out.fanin0Cp);
    Lit b = mkLit(var2, out.fanin1Cp);

    vec<Lit> lits;
    lits.clear();
    lits.push(a);
    lits.push(~y);
    _solver->addClause(lits);
    lits.clear();
    lits.push(b);
    lits.push(~y);
    _solver->addClause(lits);
    lits.clear();
    lits.push(~a);
    lits.push(~b);
    lits.push(y);
    _solver->addClause(lits);
    lits.clear();
}

void
GVSatSolver::addBoundedVerifyData(const GVNetId& id, const uint32_t& depth) {
    if (existVerifyData(id, depth)) return;
    addBoundedVerifyDataRecursively(id, depth);
}

void
GVSatSolver::addBoundedVerifyDataRecursively(const GVNetId& id, const uint32_t& depth) {
    const GV_Ntk_Type_t type = gvNtkMgr->getGateType(id);
    if (existVerifyData(id, depth)) return;
    if (GV_NTK_OBJ_PI == type) add_PI_Formula(id, depth);
    else if (GV_NTK_OBJ_FF_CS == type || GV_NTK_OBJ_FF_NS == type) {
        uint32_t newDepth = depth;
        if (depth) {
            if (GV_NTK_OBJ_FF_NS == type) newDepth -= 1;
            addBoundedVerifyDataRecursively(_ntk->getInputNetId(id, 0), newDepth);
        }
        add_FF_Formula(id, depth);
    } else if (GV_NTK_OBJ_AIG >= type) {
        // if (GV_NTK_OBJ_PO == type) {
        //     addBoundedVerifyDataRecursively(_ntk->getInputNetId(id, 0), depth);
        //     add_FF_Formula(id, depth);
        // } else
        if (GV_NTK_OBJ_AIG == type) {
            addBoundedVerifyDataRecursively(_ntk->getInputNetId(id, 0), depth);
            addBoundedVerifyDataRecursively(_ntk->getInputNetId(id, 1), depth);
            add_AND_Formula(id, depth);
        } else {
            add_FALSE_Formula(id, depth);
        }
    } else {
        assert(0);
    }
}

const bool
GVSatSolver::existVerifyData(const GVNetId& id, const uint32_t& depth) {
    return getVerifyData(id, depth);
}

#endif
