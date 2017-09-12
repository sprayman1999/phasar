/*
 * Summary.hh
 *
 *  Created on: 26.05.2017
 *      Author: philipp
 */

#ifndef SRC_ANALYSIS_IFDS_IDE_IFDSSUMMARY_HH_
#define SRC_ANALYSIS_IFDS_IDE_IFDSSUMMARY_HH_

#include <llvm/IR/Instruction.h>
#include <string>
#include <vector>
#include "FlowFunction.hh"
#include "ZeroValue.hh"
using namespace std;

template <typename D, typename N>
class IFDSSummary : FlowFunction<D> {
 private:
  N StartNode;
  N EndNode;
  vector<bool> Context;
  set<D> Outputs;
  D ZeroValue;

 public:
  IFDSSummary(N Start, N End,
              vector<bool> C, set<D> Gen, D ZV)
      : StartNode(Start),
        EndNode(End),
        Context(C),
        Outputs(Gen),
        ZeroValue(ZV) {}
  virtual ~IFDSSummary() = default;
  set<D> computeTargets(D source) override {
    if (source == ZeroValue) {
      Outputs.insert(source);
      return Outputs;
    } else {
      return {source};
    }
  }

  N getStartNode() const { return StartNode; }

  N getEndNode() const { return EndNode; }
};

#endif /* SRC_ANALYSIS_IFDS_IDE_IFDSSUMMARY_HH_ */
