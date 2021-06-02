//
// Cforall Version 1.0.0 Copyright (C) 2015 University of Waterloo
//
// The contents of this file are covered under the licence agreement in the
// file "LICENCE" distributed with Cforall.
//
// Examine.h --
//
// Author           : Andrew Beach
// Created On       : Wed Sept 2 13:57 2020
// Last Modified By : Andrew Beach
// Last Modified On : Wed Sep  8 12:08 2020
// Update Count     : 0
//

#include "SynTree/Declaration.h"

/// Check if this is a main function for a type of an aggregate kind.
DeclarationWithType * isMainFor( FunctionDecl * func, AggregateDecl::Aggregate kind );
// Returns a pointer to the parameter if true, nullptr otherwise.

/// Check if this function is a destructor for the given structure.
bool isDestructorFor( FunctionDecl * func, StructDecl * type_decl );
