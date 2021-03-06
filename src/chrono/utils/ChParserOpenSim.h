// =============================================================================
// PROJECT CHRONO - http://projectchrono.org
//
// Copyright (c) 2014 projectchrono.org
// All rights reserved.
//
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file at the top level of the distribution and at
// http://projectchrono.org/license-chrono.txt.
//
// =============================================================================
// Authors: Conlain Kelly
// =============================================================================
//
// Parser utility class for OpenSim input files.
//
// =============================================================================

#ifndef CH_PARSER_OPENSIM_H
#define CH_PARSER_OPENSIM_H

#include <functional>
#include <map>

#include "chrono/core/ChApiCE.h"
#include "chrono/physics/ChBodyAuxRef.h"
#include "chrono/physics/ChLoadContainer.h"
#include "chrono/physics/ChSystem.h"

#include "chrono_thirdparty/rapidxml/rapidxml.hpp"

namespace chrono {

namespace utils {

/// @addtogroup chrono_utils
/// @{

/// OpenSim input file parser
class ChApi ChParserOpenSim {
  public:
    enum VisType { PRIMITIVES, MESH, NONE };
    /// Report containing information about objects parsed from file
    class ChApi Report {
      public:
        /// Custom structure that holds information about a joint read in from OpenSim
        struct OpenSimJoint {
            /// Pointer to the joint so it can be modified
            std::shared_ptr<ChLink> Joint;
            /// The type listed in the .osim file
            std::string OpenSimType;
            /// The corresponding Chrono type
            std::string ChronoType;
            /// Whether or not it was loaded exactly
            bool standin;
        };
        /// List of ChBodies read in
        std::vector<std::shared_ptr<ChBody>> bodiesList;
        /// List of joints read in, their OpenSim, and Chrono representations
        std::vector<OpenSimJoint> jointsList;
        /// Print the bodies and joints read
        void Print() const;
    };

    ChParserOpenSim();
    ~ChParserOpenSim() {}

    /// Set coefficient of friction.
    /// The default value is 0.6
    void SetContactFrictionCoefficient(float friction_coefficient) { m_friction = friction_coefficient; }

    /// Set coefficient of restitution.
    /// The default value is 0.4
    void SetContactRestitutionCoefficient(float restitution_coefficient) { m_restitution = restitution_coefficient; }

    /// Set contact material properties.
    /// These values are used to calculate contact material coefficients (if the containing
    /// system is so configured and if the SMC contact method is being used).
    /// The default values are: Y = 2e5 and nu = 0.3
    void SetContactMaterialProperties(float young_modulus,  ///< [in] Young's modulus of elasticity
                                      float poisson_ratio   ///< [in] Poisson ratio
    );

    /// Set contact material coefficients.
    /// These values are used directly to compute contact forces (if the containing system
    /// is so configured and if the SMC contact method is being used).
    /// The default values are: kn=2e5, gn=40, kt=2e5, gt=20
    void SetContactMaterialCoefficients(float kn,  ///< [in] normal contact stiffness
                                        float gn,  ///< [in] normal contact damping
                                        float kt,  ///< [in] tangential contact stiffness
                                        float gt   ///< [in] tangential contact damping
    );

    /// Enable collision between bodies in this model (default: false).
    /// Set collision families (to disable collision between a body and its parent).
    void EnableCollision(int family_1 = 1,  ///< [in] First collision family
                         int family_2 = 2   ///< [in] Second collision family
    );

    /// Set body visualization type (default: NONE).
    void SetVisualizationType(VisType val) { m_visType = val; }

    /// Enable/disable verbose parsing output (default: false).
    void SetVerbose(bool val) { m_verbose = val; }

    /// Parse the specified OpenSim input file and create the model in the given system.
    void Parse(ChSystem& system,            ///< [in] containing Chrono system
               const std::string& filename  ///< [in] OpenSim input file name
    );

    /// Parse the specified OpenSim input file and create the model in a new system.
    /// Note that the created system is not deleted in the parser's destructor;
    /// rather, ownership is transferred to the caller.
    ChSystem* Parse(const std::string& filename,  ///< [in] OpenSim input file name
                    ChMaterialSurface::ContactMethod contact_method = ChMaterialSurface::NSC  ///< [in] contact method
    );

    /// Get the list of bodies in the model.
    const std::vector<std::shared_ptr<ChBodyAuxRef>>& GetBodyList() const { return m_bodyList; }

    /// Get the list of joints in the model.
    const std::vector<std::shared_ptr<ChLink>>& GetJointList() const { return m_jointList; }

    /// Get the report for this parser
    const Report& GetReport() const { return m_report; }

    /// Get print the parser's report
    void PrintReport() const { m_report.Print(); }

  private:
    /// Setup lambda table for body parsing
    void initFunctionTable();
    Report m_report;

    /// Creates load object and parses its various properties from its XML child nodes
    bool parseForce(rapidxml::xml_node<>* bodyNode, ChSystem& system, std::shared_ptr<ChLoadContainer> container);

    /// Creates body and parses its various properties from its XML child nodes
    bool parseBody(rapidxml::xml_node<>* bodyNode, ChSystem& system);

    // Initializes visualization shapes for bodies connected to each link
    void initShapes(rapidxml::xml_node<>* node, ChSystem& system);

    // Get an STL vector from a string, used to make the xml parsing cleaner
    template <typename T>
    static inline std::vector<T> strToSTLVector(const char* string) {
        std::istringstream buf(string);
        std::istream_iterator<T> beg(buf), end;
        return std::vector<T>(beg, end);
    }

    // Convert a space-delimited string into a ChVector
    template <typename T>
    static inline ChVector<T> strToChVector(const char* string) {
        auto elems = strToSTLVector<T>(string);
        return ChVector<T>(elems.at(0), elems.at(1), elems.at(2));
    }
    // Convert a lowercase string to a boolean
    static inline bool strToBool(const char* string) { return std::strcmp(string, "true") == 0; }

    // Maps child fields of a body node to functions that handle said fields
    std::map<std::string, std::function<void(rapidxml::xml_node<>*, std::shared_ptr<ChBodyAuxRef>)>> function_table;

    bool m_verbose;     ///< verbose output
    VisType m_visType;  ///< Body visualization type
    bool m_collide;     ///< Do bodies have collision shapes?
    int m_family_1;     ///< First collision family
    int m_family_2;     ///< Second collision family

    float m_friction;       ///< contact coefficient of friction
    float m_restitution;    ///< contact coefficient of restitution
    float m_young_modulus;  ///< contact material Young modulus
    float m_poisson_ratio;  ///< contact material Poisson ratio
    float m_kn;             ///< normal contact stiffness
    float m_gn;             ///< normal contact damping
    float m_kt;             ///< tangential contact stiffness
    float m_gt;             ///< tangential contact damping

    std::vector<std::shared_ptr<ChBodyAuxRef>> m_bodyList;  ///< List of bodies in model
    std::vector<std::shared_ptr<ChLink>> m_jointList;       ///< List of joints in model
};

/// @} chrono_utils

}  // end namespace utils
}  // end namespace chrono

#endif
