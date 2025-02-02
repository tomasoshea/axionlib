/*************************************************************************
 * This file is part of the REST software framework.                     *
 *                                                                       *
 * Copyright (C) 2016 GIFNA/TREX (University of Zaragoza)                *
 * For more information see http://gifna.unizar.es/trex                  *
 *                                                                       *
 * REST is free software: you can redistribute it and/or modify          *
 * it under the terms of the GNU General Public License as published by  *
 * the Free Software Foundation, either version 3 of the License, or     *
 * (at your option) any later version.                                   *
 *                                                                       *
 * REST is distributed in the hope that it will be useful,               *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the          *
 * GNU General Public License for more details.                          *
 *                                                                       *
 * You should have a copy of the GNU General Public License along with   *
 * REST in $REST_PATH/LICENSE.                                           *
 * If not, see http://www.gnu.org/licenses/.                             *
 * For the list of contributors see $REST_PATH/CREDITS.                  *
 *************************************************************************/

//////////////////////////////////////////////////////////////////////////
///
/// This process is designed to work with the most general case of the magnetic field description using the
/// help of TRestAxionMagneticField metadata class. This means that the magnetic field volume can be made of
/// several magnetic field "regions" (as described in the documentation of TRestAxionMagneticField, where each
/// "region" is defined by a separate `<addMagnetVolume...>` line in its RML configuration file. One such
/// example of generic magnetic field volume description is shown in the following figure, where the volume
/// consists of four regions labeled #1 . . . #4. In the following example the regions appear as connected
/// regions although in general they could be completely unconnected, or isolated regions. Anywhere where no
/// region is defined the field will be equal to zero.
///
/// \htmlonly <style>div.image img[src="AxionMagnetTrajectory.png"]{width:500px;}</style> \endhtmlonly
///
/// ![Schematic of the axion trajectory through the magnetic field volumes.](AxionMagnetTrajectory.png)
///
/// Here, the boundaries of each region are represented by blue lines. Also, in some (or all) regions the
/// magnetic field can be zero in its outer parts in order to define a more complex field shape inside.
/// These parts where \f$B = 0\f$ are shown in blue, while the parts of the
/// regions with \f$B \neq 0\f$ are shown in green. In this case, it is possible that the particle, right
/// after entering the region, passes through the part where \f$B = 0\f$ before traversing the section with
/// \f$B \neq 0\f$. Also, just before exiting the region, the particle can again pass through the section
/// with \f$B = 0\f$.
///
/// This process will use the profile of the transversal magnetic field component at each of the volumes
/// along the path to calculate the probability the particle is in a photon state at the end of the
/// trajectory,
///
/// In a first approach this process will be only valid for the axion propagation inside a single magnetic
/// volume, until it is confirmed the process is valid for any number of volumes.
///
/// This process requires at least the definition of a magnetic field using a TRestAxionMagneticField
/// metadata definition. Optionally, if the field uses a buffer gas we may define a TRestAxionBufferGas
/// that will include all the necessary gas properties, such as photon mass or absorption.
///
/// Two metadata parameters can be defined inside this process:
///
/// * **integrationStep** (Default:50mm): The integration length used for field integration along the particle
/// track.
/// * **bufferGasAdditionalLength** (Default:0mm): In the case we use a buffer gas we may include an
/// additional length that the particle needs to travel inside that buffer gas but outside the magnetic field
/// volume, the result will be written as an independent efficiency at the `transmission` observable inside
/// the analysis tree.
///
/// List of observables generated by this process:
///
/// * **fieldAverage**: The averaged magnetic field calculated along the particle track.
/// * **probability**: The final axion-photon conversion probability.
/// * **coherenceLength**: The lentgh of magnetic field region traversed by the particle.
/// * **transmission**: This observable will register the photon transmission produced by an additional
/// buffer gas length at the end of the magnetic region.
///
/// This process can be tested using the RML files found inside the directory
/// `/pipeline/ray-tracing/axion-field/`. The following commands will generate plots with different
/// distributions containing different axion-photon probability calculations.
///
/// \code
/// restManager --c photonConversion.rml
/// restManager --c plots.rml --f AxionPhotonProbability.root
/// \endcode
///
/// The previous commands were used to generate the following plots where a total of 100,000 events
/// were generated.
///
/// \htmlonly <style>div.image img[src="axionFieldPlots.png"]{width:800px;}</style> \endhtmlonly
///
/// ![Ray-tracing distributions of probability, field average and coherence length](axionFieldPlots.png)
///
///--------------------------------------------------------------------------
///
/// RESTsoft - Software for Rare Event Searches with TPCs
///
/// History of developments:
///
/// 2019-March:  First prototyping of the process.
///              Javier Galan
///
/// 2019-July:   Implementation of boundaries and magnetic field evaluation.
///              Eve Pachoud
///
/// 2020-March:  Review and validation of this process.
///              Javier Galan and Krešimir Jakovčić
///
/// 2022-November:  Finally including non-homogeneous field integration
///                 Javier Galan
///
///
/// \class      TRestAxionFieldPropagationProcess
/// \author     Javier Galan <javier.galan@unizar.es>
/// \author     Krešimir Jakovčić <kjakov@irb.hr>
///
/// <hr>
///
#include "TRestAxionFieldPropagationProcess.h"

#include <TVectorD.h>

#include <numeric>
using namespace std;

ClassImp(TRestAxionFieldPropagationProcess);

///////////////////////////////////////////////
/// \brief Default constructor
///
TRestAxionFieldPropagationProcess::TRestAxionFieldPropagationProcess() { Initialize(); }

///////////////////////////////////////////////
/// \brief Constructor loading data from a config file
///
/// If no configuration path is defined using TRestMetadata::SetConfigFilePath
/// the path to the config file must be specified using full path, absolute or relative.
///
/// The default behaviour is that the config file must be specified with
/// full path, absolute or relative.
///
/// \param cfgFileName A const char* giving the path to an RML file.
///
TRestAxionFieldPropagationProcess::TRestAxionFieldPropagationProcess(char* cfgFileName) { Initialize(); }

///////////////////////////////////////////////
/// \brief Default destructor
///
TRestAxionFieldPropagationProcess::~TRestAxionFieldPropagationProcess() { delete fAxionEvent; }

///////////////////////////////////////////////
/// \brief Function to initialize input/output event members and define the section name
///
void TRestAxionFieldPropagationProcess::Initialize() {
    SetSectionName(this->ClassName());
    SetLibraryVersion(LIBRARY_VERSION);

    fAxionEvent = new TRestAxionEvent();
}

///////////////////////////////////////////////
/// \brief Process initialization. Data members that require initialization just before start processing
/// should be initialized here.
///
void TRestAxionFieldPropagationProcess::InitProcess() {
    RESTDebug << "Entering ... TRestAxionFieldPropagationProcess::InitProcess" << RESTendl;

    fMagneticField = (TRestAxionMagneticField*)this->GetMetadata("TRestAxionMagneticField");

    RESTDebug << "Magnetic field : " << fMagneticField << RESTendl;

    if (!fMagneticField) {
        RESTError << "TRestAxionFieldPropagationprocess. Magnetic Field was not defined!" << RESTendl;
        exit(0);
    }

    if (!fAxionField) {
        fAxionField = new TRestAxionField();

        fBufferGas = (TRestAxionBufferGas*)this->GetMetadata("TRestAxionBufferGas");
        if (fBufferGas)
            fAxionField->AssignBufferGas(fBufferGas);
        else
            fBufferGasAdditionalLength = 0;
    }

    RESTDebug << "Axion-field : " << fAxionField << RESTendl;
    RESTDebug << "Buffer gas : " << fBufferGas << RESTendl;
}

TRestEvent* TRestAxionFieldPropagationProcess::ProcessEvent(TRestEvent* evInput) {
    fAxionEvent = (TRestAxionEvent*)evInput;

    RESTDebug << "TRestAxionFieldPropagationProcess::ProcessEvent : " << fAxionEvent->GetID() << RESTendl;

    std::vector<TVector3> trackBounds =
        fMagneticField->GetFieldBoundaries(fAxionEvent->GetPosition(), fAxionEvent->GetDirection());

    Double_t prob = 0;
    Double_t lCoh = 0;
    Double_t transmission = 1;
    Double_t fieldAverage = 0;
    if (trackBounds.size() == 2) {
        RESTDebug << "-- Track bounds" << RESTendl;
        RESTDebug << "X1:" << trackBounds[0].X() << " Y1: " << trackBounds[0].Y()
                  << " Z1: " << trackBounds[0].Z() << RESTendl;
        RESTDebug << "X2:" << trackBounds[1].X() << " Y2: " << trackBounds[1].Y()
                  << " Z2: " << trackBounds[1].Z() << RESTendl;
        std::vector<Double_t> bProfile = fMagneticField->GetTransversalComponentAlongPath(
            trackBounds[0], trackBounds[1], fIntegrationStep);

        fieldAverage = std::accumulate(bProfile.begin(), bProfile.end(), 0.0) / bProfile.size();

        Double_t Ea = fAxionEvent->GetEnergy();
        Double_t ma = fAxionEvent->GetMass();

        prob = fAxionField->GammaTransmissionProbability(bProfile, fIntegrationStep, Ea, ma);

        lCoh = (bProfile.size() - 1) * fIntegrationStep;

        if (fBufferGas && fBufferGasAdditionalLength > 0) {
            Double_t Gamma = fBufferGas->GetPhotonAbsorptionLength(Ea);  // cm-1
            Double_t GammaL = Gamma * lCoh * units("cm");
            transmission = exp(-GammaL);
        }
    } else {
        SetWarning("TRestAxionFieldPropagationProcess. Track does not cross the field volume!", false);
    }

    RESTDebug << " --- Process observables: " << RESTendl;
    RESTDebug << "Field average: " << fieldAverage << " T" << RESTendl;
    RESTDebug << "Probability: " << prob << " T" << RESTendl;
    RESTDebug << "Coherence length: " << lCoh << " mm" << RESTendl;
    RESTDebug << "Transmission: " << transmission << " mm" << RESTendl;

    SetObservableValue("fieldAverage", fieldAverage);
    SetObservableValue("probability", prob);
    SetObservableValue("coherenceLength", lCoh);
    SetObservableValue("transmission", transmission);

    if (GetVerboseLevel() >= TRestStringOutput::REST_Verbose_Level::REST_Debug) fAxionEvent->PrintEvent();

    /// Missing to propagate the axion to the end of magnet bore?
    /// May not be necessary, it can be done by TRestAxionTransportProcess if user needs any process
    /// that changes direction, this is done for example by optics processes internally

    return fAxionEvent;
}
