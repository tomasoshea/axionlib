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
/// TRestAxionPhotonConversion is a class used to ...
///
/// ... for the moment we implement here the equations from van Bibber paper
///
/// TODO. Create an appropriate documentation here.
///
///--------------------------------------------------------------------------
///
/// RESTsoft - Software for Rare Event Searches with TPCs
///
/// History of developments:
///
/// 2019-March: First concept and implementation of TRestAxionPhotonConversion class.
///             Javier Galan
///
/// \class      TRestAxionPhotonConversion
/// \author     Javier Galan
///
/// <hr>
///
#include "TRestAxionPhotonConversion.h"
#include <TVectorD.h>
#include "TComplex.h"
#include "TH1F.h"
using namespace std;

using namespace REST_Physics;

#include "mpreal.h"
using mpfr::mpreal;

ClassImp(TRestAxionPhotonConversion);

///////////////////////////////////////////////
/// \brief Default constructor
///
TRestAxionPhotonConversion::TRestAxionPhotonConversion() { Initialize(); }

///////////////////////////////////////////////
/// \brief Default destructor
///
TRestAxionPhotonConversion::~TRestAxionPhotonConversion() {}

///////////////////////////////////////////////
/// \brief Initialization of TRestAxionPhotonConversion class
///
/// It will set the default real precision to be used with mpfr types. Now it is 100, not sure if 100 is
/// the number of digits, we need to optimize this in future.
///
void TRestAxionPhotonConversion::Initialize() { mpreal::set_default_prec(mpfr::digits2bits(100)); }

///////////////////////////////////////////////
/// \brief Performs the calculation of (BL) factor in natural units.
///
/// `Lcoh` should be expressed in `mm`, and `Bmag` in `T`.
/// The result will be given for an axion-photon coupling of 10^{-10} GeV^{-1}
///
double TRestAxionPhotonConversion::BL(Double_t Bmag, Double_t Lcoh) {
    Double_t lengthInMeters = Lcoh / 1000.;

    Double_t tm = lightSpeed / naturalElectron * 1.0e-9;  // GeV
    Double_t sol = lengthInMeters * Bmag * tm;
    sol = sol * 1.0e-10;

    return sol;
}

///////////////////////////////////////////////
/// \brief Performs the calculation of (BL/2)^2 factor in natural units.
///
/// `Lcoh` should be expressed in `mm`, and `Bmag` in `T`.
/// The result will be given for an axion-photon coupling of 10^{-10} GeV^{-1}
///
double TRestAxionPhotonConversion::BLHalfSquared(Double_t Bmag, Double_t Lcoh)  // (BL/2)**2
{
    Double_t lengthInMeters = Lcoh / 1000.;

    Double_t tm = lightSpeed / naturalElectron * 1.0e-9;  // gev
    Double_t sol = lengthInMeters * Bmag * tm / 2;
    sol = sol * sol * 1.0e-20;

    return sol;
}

///////////////////////////////////////////////
/// \brief Performs the calculation of axion-photon conversion probability using directly the van Bibber
/// relation.
///
/// m_gamma will be obtainned from buffer gas definition. If no buffer gas has been assigned the medium
/// will be assumed to be vacuum.
///
/// Ea in keV, ma in eV, mgamma in eV, Lcoh in mm, Bmag in T
///
Double_t TRestAxionPhotonConversion::GammaTransmissionProbability(Double_t Ea, Double_t Bmag, Double_t ma,
                                                                  Double_t Lcoh) {
    mpreal axionMass = ma;
    mpreal cohLength = Lcoh / 1000.;  // Default REST units are mm;
    mpreal bMag = Bmag;

    mpreal photonMass = 0.;
    if (fBufferGas) photonMass = fBufferGas->GetPhotonMass(Ea);

    debug << "+--------------------------------------------------------------------------+" << endl;
    debug << " TRestAxionPhotonConversion::GammaTransmissionProbability. Parameter summary" << endl;
    debug << " Photon mass : " << photonMass << " eV" << endl;
    debug << " Axion mass : " << ma << " eV" << endl;
    debug << " Axion energy : " << Ea << " keV" << endl;
    debug << " Lcoh : " << Lcoh << " mm" << endl;
    debug << " Bmag : " << Bmag << " T" << endl;
    debug << "+--------------------------------------------------------------------------+" << endl;

    if (ma == 0.0 && photonMass == 0.0) return BLHalfSquared(Bmag, Lcoh);

    mpreal q = (ma * ma - photonMass * photonMass) / 2. / Ea / 1000.0;
    mpreal l = cohLength * PhMeterIneV;
    mpreal phi = q * l;

    mpreal Gamma = 0.;
    if (fBufferGas) Gamma = fBufferGas->GetPhotonAbsorptionLength(Ea);  // cm-1
    mpreal GammaL = Gamma * cohLength * 100;

    debug << "+------------------------+" << endl;
    debug << " Intermediate calculations" << endl;
    debug << " q : " << q << " eV" << endl;
    debug << " l : " << l << " eV-1" << endl;
    debug << " phi : " << phi << endl;
    debug << "Gamma : " << Gamma << endl;
    debug << "GammaL : " << GammaL << endl;
    debug << "+------------------------+" << endl;

    mpreal MFactor = phi * phi + GammaL * GammaL / 4.0;
    MFactor = 1.0 / MFactor;

    debug << "Mfactor : " << MFactor << endl;
    debug << "(BL/2)^2 : " << BLHalfSquared(Bmag, Lcoh) << endl;
    debug << "cos(phi) : " << cos(phi) << endl;
    debug << "Exp(-GammaL) : " << exp(-GammaL) << endl;

    double sol =
        (double)(MFactor * BLHalfSquared(Bmag, Lcoh) * (1 + exp(-GammaL) - 2 * exp(-GammaL / 2) * cos(phi)));

    debug << "Axion-photon transmission probability : " << sol << endl;

    return sol;
}

///////////////////////////////////////////////
/// \brief Performs the calculation of axion-photon conversion probability using directly the van Bibber
/// relation.
///
/// It does it for an arbitrary magnetic field vector.
///
/// This method might  become obsolete if we accept that the magnetic field average is enough.
///
Double_t TRestAxionPhotonConversion::GammaTransmissionProbability(Double_t Ea, TVectorD B, Double_t ma,
                                                                  Double_t Lcoh) {
    Double_t cohLength = Lcoh / 1000.;  // Default REST units are mm;
    Double_t photonMass = 0.;
    if (!fBufferGas) {
        warning << "TRestAxionPhotonConversion::GammaTransmissionProbability. No buffer gas definition found!"
                << endl;
        warning << "Assuming vacuum medium. mgamma = 0" << endl;
    } else {
        photonMass = fBufferGas->GetPhotonMass(Ea);
    }

    debug << "+--------------------------------------------------------------------------+" << endl;
    debug << " TRestAxionPhotonConversion::GammaTransmissionProbability. Parameter summary" << endl;
    debug << " Photon mass : " << photonMass << " eV" << endl;
    debug << " Axion mass : " << ma << " eV" << endl;
    debug << " Axion energy : " << Ea << " keV" << endl;
    debug << " Lcoh : " << Lcoh << " mm" << endl;
    debug << "+--------------------------------------------------------------------------+" << endl;

    Double_t q = (ma * ma - photonMass * photonMass) / 2. / Ea / 1000.0;
    Double_t l = cohLength * PhMeterIneV;
    Double_t phi = q * l;

    Double_t Gamma = 0.;
    if (fBufferGas) Gamma = fBufferGas->GetPhotonAbsorptionLength(Ea);  // cm-1

    Double_t GammaL = Gamma * cohLength * 100;

    debug << "+------------------------+" << endl;
    debug << " Intermediate calculations" << endl;
    debug << " q : " << q << " eV" << endl;
    debug << " l : " << l << " eV-1" << endl;
    debug << " phi : " << phi << endl;
    debug << "Gamma : " << Gamma << endl;
    debug << "GammaL : " << GammaL << endl;
    debug << "+------------------------+" << endl;

    // ***** Computation of the prefactor (1/2M)^2***** //
    Double_t tm = lightSpeed / naturalElectron * 1.0e-9;
    Double_t Factor = tm / 2.0;
    Factor = Factor * Factor * 1.0e-20;

    debug << "+------------------------+" << endl;
    debug << "Factor (1/2M)^2 :" << Factor << endl;
    debug << "+------------------------+" << endl;

    // ***** Computation of the integral ***** //
    Double_t xmin = 0.0;
    Double_t xmax = cohLength;

    Int_t size = B.GetNoElements();
    Int_t N = size - 1;
    TVectorD ind(N + 1);
    TVectorD Bx(N + 1);
    TVectorD By(N + 1);
    Double_t b;
    for (Int_t i = 0; i <= N; i++) {
        b = B[i];
        Bx[i] = b * TMath::Exp((GammaL / 2.0) * (Double_t(i) / Double_t(N) - 1.0)) *
                TMath::Cos(-cohLength * PhMeterIneV * (Double_t(i) / Double_t(N)) * q);
        By[i] = b * TMath::Exp((GammaL / 2.0) * (Double_t(i) / Double_t(N) - 1.0)) *
                TMath::Sin(-cohLength * PhMeterIneV * (Double_t(i) / Double_t(N)) * q);
        ind[i] = cohLength * (Double_t(i) / Double_t(N));
    }

    TH1F* hx = new TH1F("hx", "hx", N + 1, xmin, xmax);
    TH1F* hy = new TH1F("hy", "hy", N + 1, xmin, xmax);
    for (Int_t i = 0; i <= N; i++) {
        hx->Fill(ind[i], Bx[i]);
        hy->Fill(ind[i], By[i]);
    }
    TAxis* axis = hx->GetXaxis();

    int bmin = axis->FindBin(xmin);
    int bmax = axis->FindBin(xmax);

    Double_t integralx = hx->Integral(bmin, bmax);
    integralx = integralx * (xmax - xmin) / (bmax - bmin);
    Double_t integraly = hy->Integral(bmin, bmax);
    integraly = integraly * (xmax - xmin) / (bmax - bmin);

    TComplex inte = operator+(integralx, operator*(integraly, TComplex::I()));
    Double_t sol;
    sol = inte.operator*(TComplex::Conjugate(inte));
    sol = Factor * sol;

    debug << "+------------------------+" << endl;
    debug << "Axion-photon transmission probability : " << sol << endl;
    debug << "+------------------------+" << endl;

    delete hx;
    delete hy;
    return sol;
}

