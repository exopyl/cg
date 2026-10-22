#pragma once
//
//  Compteur et injection d'echec sur les allocations du THREAD APPELANT.
//
// Definis dans tu_cgarchi_gothic_print_faults.cpp, qui remplace l'operator new
// global de TU_faults ; les autres threads ne sont jamais comptes ni touches.
//
#include <functional>

// Allocations du thread appelant pendant f ().
long CountThreadAllocations (const std::function<void ()> &f);

// La countdown-ieme allocation suivante du thread appelant leve std::bad_alloc,
// une fois. DisarmThreadAllocationFailure rend vrai si elle a eu lieu.
void ArmThreadAllocationFailure (long countdown);
bool DisarmThreadAllocationFailure ();

// Vrai sous une bibliotheque standard de debogage, ou des operations noexcept
// allouent : un echec injecte y finirait en std::terminate.
bool DebugStandardLibrary ();
