#pragma once

namespace helpers {

class Deferred {
public:
  Deferred();
  bool IsDiferred();
  void SetDiferred(bool new_diferred);

  bool GetDeferred();
  void ZapDeferred();
  bool PutDeffered();

private:
  bool diferred_;
};

} // namespace helpers