#pragma once

/*
Meaning of flags on verbose:
        1	-g global variable values
        2	-l local variable values
        4	-p all process actions
        8	-r receives
        16	-s sends
        32	-v verbose
        64	-w very verbose
*/

namespace utils::verbose {

class Flags {
private:
  Flags();
  Flags(const Flags &);
  Flags &operator=(Flags &);

public:
    bool NeedToPrintGlobalVariables();//1
    bool NeedToPrintLocalVariables();//2
    bool NeedToPrintAllProcessActions();//4
    bool NeedToPrintReceives();//8
    bool NeedToPrintSends();//16
    bool NeedToPrintVerbose();//32
    bool NeedToPrintVeryVerbose();//64

    bool Active();
    bool Clean();
    bool Activate();

    void SetNeedToPrintGlobalVariables();
    void SetNeedToPrintLocalVariables( );
    void SetNeedToPrintAllProcessActions( );
    void SetNeedToPrintReceives( );
    void SetNeedToPrintSends( );
    void SetNeedToPrintVerbose( );
    void SetNeedToPrintVeryVerbose( );
  static Flags &getInstance() {
    static Flags instance;
    return instance;
  }

private:
    bool clean_;
    bool need_to_print_global_variables_;
    bool need_to_print_local_variables_;
    bool need_to_print_all_process_actions_;
    bool need_to_print_receives_;
    bool need_to_print_sends_;
    bool need_to_print_verbose_;
    bool need_to_print_very_verbose_;
};

} // namespace utils::verbose