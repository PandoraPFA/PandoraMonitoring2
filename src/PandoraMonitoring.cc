/**
 *  @file   PandoraMonitoring/src/PandoraMonitoring.cc
 * 
 *  @brief  Implementation of the pandora monitoring class.
 * 
 *  $Log: $
 */

// Pandora include files
#include "Helpers/GeometryHelper.h"

#include "Objects/CaloHit.h"
#include "Objects/CartesianVector.h"
#include "Objects/Cluster.h"
#include "Objects/DetectorGap.h"
#include "Objects/Histograms.h"
#include "Objects/OrderedCaloHitList.h"
#include "Objects/ParticleFlowObject.h"
#include "Objects/Track.h"
#include "Objects/MCParticle.h"

#include "Pandora/PdgTable.h"

// ROOT include files
#include "TCanvas.h"
#include "TFile.h"
#include "TH1F.h"
#include "TH2F.h"
#include "TROOT.h"
#include "TStyle.h"
#include "TTree.h"

#include <TEveManager.h>
#include <TEveEventManager.h>
#include <TEveViewer.h>
#include <TEvePointSet.h>
#include <TEveArrow.h>
#include <TEveRGBAPalette.h>
#include <TGLViewer.h>

#include <TGeoXtru.h>
#include <TEveGeoShapeExtract.h>
#include <TEveGeoShape.h>

#include <TGeometry.h>
#include <TGeoMaterial.h>
#include <TGeoManager.h>
#include <TEveGeoNode.h>

#include "TEveTrackPropagator.h"
#include "TEveTrack.h"
#include "TEveVSDStructs.h" // for TEveRecTrack
#include "TParticlePDG.h"

#include <TSystem.h>
#include <TGeoManager.h>
#include <TGeoXtru.h>
#include <TGeoMatrix.h>
#include <TEveManager.h>
#include <TEveGeoNode.h>
#include <TGeoTube.h>
#include <TEveBoxSet.h>
#include <TGeoCompositeShape.h>
#include <TEveScene.h>

#include "PandoraMonitoring.h"

#include <vector>
#include <assert.h>
#include <math.h>
#include <iostream>
#include <cmath>
#include <fcntl.h>
#include <limits>
#include <algorithm>

using namespace pandora;

namespace pandora_monitoring
{

bool  PandoraMonitoring::m_instanceFlag = false;
bool  PandoraMonitoring::m_eveInitialized = false;
bool  PandoraMonitoring::m_openEveEvent = false;
float PandoraMonitoring::m_scalingFactor = 0.1;
int   PandoraMonitoring::m_eventDisplayCounter = 0;

PandoraMonitoring* PandoraMonitoring::m_pPandoraMonitoring = NULL;

//------------------------------------------------------------------------------------------------------------------------------------------

PandoraMonitoring::~PandoraMonitoring()
{
    m_treeWrapper.Clear();

    if (m_eveInitialized)
    {
        TEveManager::Terminate();
        gSystem->ProcessEvents();
    }

    m_pApplication->Terminate(0);
}

//------------------------------------------------------------------------------------------------------------------------------------------

void PandoraMonitoring::DeleteInstance()
{
    delete GetInstance();
}

//------------------------------------------------------------------------------------------------------------------------------------------

PandoraMonitoring *PandoraMonitoring::GetInstance()
{
    if (!m_instanceFlag)
    {
        m_pPandoraMonitoring = new PandoraMonitoring();
        m_instanceFlag = true;
        TColor::CreateColorWheel();
        gStyle->SetPalette(1);
        gStyle->SetNumberContours(99);
    }

    return m_pPandoraMonitoring;
}

//------------------------------------------------------------------------------------------------------------------------------------------

void PandoraMonitoring::Create1DHistogram(const std::string &name, const std::string &title, int nBinsX, float xLow, float xUp,
    const std::string &xAxisTitle, const std::string &yAxisTitle)
{
    if (m_histogramMap.end() != m_histogramMap.find(name))
    {
        std::cout << "PandoraMonitoring::Create1DHistogram, error: Histogram with name '"<< name <<"' already exists." << std::endl;
        throw std::exception();
    }

    TH1F *pTH1F = new TH1F(name.c_str(), title.c_str(), nBinsX, xLow, xUp);

    if (!xAxisTitle.empty())
        pTH1F->GetXaxis()->SetTitle(xAxisTitle.c_str());

    if (!yAxisTitle.empty())
        pTH1F->GetYaxis()->SetTitle(yAxisTitle.c_str());

    m_histogramMap.insert(HistogramMap::value_type(name, pTH1F));
}

//------------------------------------------------------------------------------------------------------------------------------------------

void PandoraMonitoring::Create2DHistogram(const std::string &name, const std::string &title, int nBinsX, float xLow, float xUp, int nBinsY,
    double yLow, double yUp, const std::string &xAxisTitle, const std::string &yAxisTitle)
{
    if (m_histogramMap.end() != m_histogramMap.find(name))
    {
        std::cout << "PandoraMonitoring::Create2DHistogram, error: Histogram with name '"<< name <<"' already exists." << std::endl;
        throw std::exception();
    }

    TH2F *pTH2F = new TH2F(name.c_str(), title.c_str(), nBinsX, xLow, xUp, nBinsY, yLow, yUp);

    if (!xAxisTitle.empty())
        pTH2F->GetXaxis()->SetTitle(xAxisTitle.c_str());

    if (!yAxisTitle.empty())
        pTH2F->GetYaxis()->SetTitle(yAxisTitle.c_str());

    m_histogramMap.insert(HistogramMap::value_type(name, pTH2F));
}

//------------------------------------------------------------------------------------------------------------------------------------------

void PandoraMonitoring::Fill1DHistogram(const std::string &name, float xValue, float weight)
{
    HistogramMap::iterator iter = m_histogramMap.find(name);

    if (m_histogramMap.end() == iter)
    {
        std::cout << "PandoraMonitoring::Fill1DHistogram, error: No histogram with name '"<< name <<"' exists." << std::endl;
        throw std::exception();
    }

    iter->second->Fill(xValue, weight);
}

//------------------------------------------------------------------------------------------------------------------------------------------

void PandoraMonitoring::Fill2DHistogram(const std::string &name, float xValue, float yValue, float weight)
{
    HistogramMap::iterator iter = m_histogramMap.find(name);

    if (m_histogramMap.end() == iter)
    {
        std::cout << "PandoraMonitoring::Fill2DHistogram, error: No histogram with name '"<< name <<"' exists." << std::endl;
        throw std::exception();
    }

    TH2F *pTH2F = dynamic_cast<TH2F *>(iter->second);

    if (NULL == pTH2F)
        throw std::exception();

    pTH2F->Fill(xValue, yValue, weight);
}

//------------------------------------------------------------------------------------------------------------------------------------------

void PandoraMonitoring::AddMultiplyOrDivideHistograms(const std::string &nameHisto0, const std::string &nameHisto1, double coeff0,
    double coeff1, bool add, bool multiply )
{
    HistogramMap::iterator iter0 = m_histogramMap.find(nameHisto0);
    if (m_histogramMap.end() == iter0)
    {
        std::cout << "PandoraMonitoring::Fill2DHistogram, error: No histogram with name '"<< nameHisto0 <<"' exists." << std::endl;
        throw std::exception();
    }

    HistogramMap::iterator iter1 = m_histogramMap.find(nameHisto1);
    if (m_histogramMap.end() == iter1)
    {
        std::cout << "PandoraMonitoring::Fill2DHistogram, error: No histogram with name '"<< nameHisto1 <<"' exists." << std::endl;
        throw std::exception();
    }

    TH1 *pHisto0 = iter0->second;
    TH1 *pHisto1 = iter1->second;

    if ((NULL == pHisto0) || (NULL == pHisto1))
        throw std::exception();

    if (add)
    {
        pHisto0->Add(pHisto0,pHisto1,coeff0,coeff1);
    }
    else if (multiply)
    {
        pHisto0->Multiply(pHisto0,pHisto1,coeff0,coeff1);
    }
    else
    {
        pHisto0->Divide(pHisto0,pHisto1,coeff0,coeff1);
    }
}

//------------------------------------------------------------------------------------------------------------------------------------------

void PandoraMonitoring::DrawHistogram(const std::string &name, const std::string &options) const
{
    HistogramMap::const_iterator iter = m_histogramMap.find(name);

    if (m_histogramMap.end() == iter)
    {
        std::cout << "PandoraMonitoring::DrawHistogram, error: No histogram with name '"<< name <<"' exists." << std::endl;
        throw std::exception();
    }

    TCanvas *pCanvas = new TCanvas("PandoraMonitoring", "PandoraMonitoring", 750, 750);
    pCanvas->SetFillColor(kWhite);
    pCanvas->SetHighLightColor(kWhite);
    pCanvas->Draw();

    iter->second->Draw(options.c_str());
    this->Pause();

    delete pCanvas;
}

//------------------------------------------------------------------------------------------------------------------------------------------

template <typename T>
void PandoraMonitoring::DrawPandoraHistogram(const T &t, const std::string &options)
{
    throw pandora::StatusCodeException(pandora::STATUS_CODE_FAILURE);
}

template <>
void PandoraMonitoring::DrawPandoraHistogram(const pandora::Histogram &histogram, const std::string &options)
{
    const std::string histogramName("PandoraHistogram");
    TH1F *pTH1F = new TH1F(histogramName.c_str(), histogramName.c_str(), histogram.GetNBinsX(), histogram.GetXLow(), histogram.GetXHigh());

    for (int xBin = -1, xBinEnd = histogram.GetNBinsX(); xBin <= xBinEnd; ++xBin)
    {
        // Beware bin offset between ROOT and Pandora histograms
        pTH1F->SetBinContent(xBin + 1, histogram.GetBinContent(xBin));
    }

    TCanvas *pCanvas = new TCanvas("PandoraMonitoring", "PandoraMonitoring", 750, 750);
    pCanvas->SetFillColor(kWhite);
    pCanvas->SetHighLightColor(kWhite);
    pCanvas->Draw();

    pTH1F->Draw(options.c_str());
    this->Pause();

    delete pCanvas;
    delete pTH1F;
}

template <>
void PandoraMonitoring::DrawPandoraHistogram(const pandora::TwoDHistogram &twoDHistogram, const std::string &options)
{
    const std::string histogramName("PandoraHistogram");
    TH2F *pTH2F = new TH2F(histogramName.c_str(), histogramName.c_str(), twoDHistogram.GetNBinsX(), twoDHistogram.GetXLow(),
        twoDHistogram.GetXHigh(), twoDHistogram.GetNBinsY(), twoDHistogram.GetYLow(), twoDHistogram.GetYHigh());

    for (int xBin = -1, xBinEnd = twoDHistogram.GetNBinsX(); xBin <= xBinEnd; ++xBin)
    {
        for (int yBin = -1, yBinEnd = twoDHistogram.GetNBinsY(); yBin <= yBinEnd; ++yBin)
        {
            // Beware bin offset between ROOT and Pandora histograms
            pTH2F->SetBinContent(xBin + 1, yBin + 1, twoDHistogram.GetBinContent(xBin, yBin));
        }
    }

    TCanvas *pCanvas = new TCanvas("PandoraMonitoring", "PandoraMonitoring", 750, 750);
    pCanvas->SetFillColor(kWhite);
    pCanvas->SetHighLightColor(kWhite);
    pCanvas->Draw();

    pTH2F->Draw(options.c_str());
    this->Pause();

    delete pCanvas;
    delete pTH2F;
}

//------------------------------------------------------------------------------------------------------------------------------------------

void PandoraMonitoring::SaveAndCloseHistogram(const std::string &name, const std::string &fileName, const std::string &fileOptions)
{
    HistogramMap::iterator iter = m_histogramMap.find(name);

    if (m_histogramMap.end() == iter)
    {
        std::cout << "PandoraMonitoring::SaveHistogram, error: No histogram with name '" << name << "' exists." << std::endl;
        throw std::exception();
    }

    TFile* pTFile = new TFile(fileName.c_str(), fileOptions.c_str());

    iter->second->SetDirectory(pTFile);
    iter->second->Write(name.c_str(), TObject::kOverwrite);
    delete iter->second;

    pTFile->Write();
    pTFile->Close();
    m_histogramMap.erase(iter);

    delete pTFile;
}

//------------------------------------------------------------------------------------------------------------------------------------------

void PandoraMonitoring::DeleteHistogram(const std::string &name)
{
    HistogramMap::iterator iter = m_histogramMap.find(name);

    if (m_histogramMap.end() == iter)
    {
        std::cout << "PandoraMonitoring::DeleteHistogram, error: No histogram with name '"<< name <<"' exists." << std::endl;
        throw std::exception();
    }

    delete iter->second;
    m_histogramMap.erase(iter);
}

//------------------------------------------------------------------------------------------------------------------------------------------

template <typename VariableType>
void PandoraMonitoring::SetTreeVariable(const std::string &treeName, const std::string &variableName, VariableType variable)
{
    m_treeWrapper.Set(treeName, variableName, variable);
}

// instantiations of this template member function for the permitted types
template void PandoraMonitoring::SetTreeVariable(const std::string&, const std::string&, float  variable);
template void PandoraMonitoring::SetTreeVariable(const std::string&, const std::string&, int    variable);
template void PandoraMonitoring::SetTreeVariable(const std::string&, const std::string&, double variable);

template void PandoraMonitoring::SetTreeVariable(const std::string&, const std::string&, std::vector<float>*  variable);
template void PandoraMonitoring::SetTreeVariable(const std::string&, const std::string&, std::vector<int>*    variable);
template void PandoraMonitoring::SetTreeVariable(const std::string&, const std::string&, std::vector<double>* variable);

//------------------------------------------------------------------------------------------------------------------------------------------

void PandoraMonitoring::FillTree(const std::string &treeName)
{
    try
    {
        m_treeWrapper.Fill(treeName);
    }
    catch(TTreeWrapper::TreeNotFoundError& excpt)
    {
        std::cout << "PandoraMonitoring::FillTree, error: No tree with name '" << treeName <<"' exists." << std::endl;
    }
    catch(...)
    {
        std::cout << "PandoraMonitoring::FillTree, unknown error for tree with name '" << treeName <<"'." << std::endl;
    }
}

//------------------------------------------------------------------------------------------------------------------------------------------

void PandoraMonitoring::PrintTree(const std::string &treeName)
{
    try
    {
        m_treeWrapper.Print(treeName);
    }
    catch(TTreeWrapper::TreeNotFoundError& excpt)
    {
        std::cout << "PandoraMonitoring::PrintTree, error: No tree with name '" << treeName <<"' exists." << std::endl;
    }
    catch(...)
    {
        std::cout << "PandoraMonitoring::PrintTree, unknown error for tree with name '" << treeName <<"'." << std::endl;
    }
}

//------------------------------------------------------------------------------------------------------------------------------------------

void PandoraMonitoring::ScanTree(const std::string &treeName)
{
    try
    {
        m_treeWrapper.Scan(treeName);
    }
    catch(TTreeWrapper::TreeNotFoundError& excpt)
    {
        std::cout << "PandoraMonitoring::ScanTree, error: No tree with name '" << treeName <<"' exists." << std::endl;
    }
    catch(...)
    {
        std::cout << "PandoraMonitoring::ScanTree, unknown error for tree with name '" << treeName <<"'." << std::endl;
    }
}

//------------------------------------------------------------------------------------------------------------------------------------------

void PandoraMonitoring::SaveTree(const std::string &treeName, const std::string &fileName, const std::string &fileOptions)
{
    try
    {
       TTree*& tree = m_treeWrapper.GetTree(treeName);

       TFile* pTFile = new TFile(fileName.c_str(), fileOptions.c_str());

       tree->SetDirectory(pTFile);
       tree->Write(TString(treeName.c_str()), TObject::kOverwrite);

       pTFile->Close();

       tree = 0; // pointer not valid any more

       delete pTFile;
    }
    catch(TTreeWrapper::TreeNotFoundError& excpt)
    {
        std::cout << "PandoraMonitoring::SaveTree, error: No tree with name '" << treeName <<"' exists." << std::endl;
    }
    catch(...)
    {
        std::cout << "PandoraMonitoring::SaveTree, unknown error for tree with name '" << treeName <<"'." << std::endl;
    }
}

//------------------------------------------------------------------------------------------------------------------------------------------

void PandoraMonitoring::Pause() const
{
#ifdef __unix__
    std::cout << "Press return to continue ..." << std::endl;
    int flag = fcntl(1, F_GETFL, 0);

    int key = 0;
    while(true)
    {
        gSystem->ProcessEvents();
        fcntl(1, F_SETFL, flag | O_NONBLOCK);
        key = getchar();

        if((key == '\n') || (key == '\r'))
            break;

        usleep(1000);
    }

    fcntl(1, F_SETFL, flag);
#else
    std::cout << "PandoraMonitoring::Pause() is only implemented for unix operating systems." << std::endl;
#endif
}

//------------------------------------------------------------------------------------------------------------------------------------------

void PandoraMonitoring::InitializeEve(Char_t transparency)
{
    std::stringstream sstr;
    sstr << "Event Display " << m_eventDisplayCounter;

    if (PandoraMonitoring::m_eveInitialized)
    {
        TEveEventManager *pCurrentEvent = gEve->GetCurrentEvent();

        if (pCurrentEvent)
        {
            pCurrentEvent->SetElementNameTitle(sstr.str().c_str(),sstr.str().c_str());
        }

        if (!m_openEveEvent)
        {
            gEve->AddEvent(new TEveEventManager(sstr.str().c_str(),sstr.str().c_str()));
            m_openEveEvent = true;
            m_eventDisplayCounter++;
        }

        return;
    }

    gSystem->Load("libGeom");
    TGeoManager *pGeoManager = new TGeoManager("DetectorGeometry", "detector geometry");

    //--- define some materials
    TGeoMaterial *pVacuumMaterial = new TGeoMaterial("Vacuum", 0, 0, 0); // dummy material
    TGeoMaterial *pAluminiumMaterial = new TGeoMaterial("Aluminium", 26.98, 13, 2.7); // dummy material

    //--- define some media
    TGeoMedium *pVacuum = new TGeoMedium("Vacuum",1, pVacuumMaterial);
    TGeoMedium *pAluminium = new TGeoMedium("Aluminium",2, pAluminiumMaterial);

    //--- make the top container volume
    TGeoVolume *pMainDetectorVolume = pGeoManager->MakeBox("Detector", pVacuum, 1000., 1000., 100.);
    pGeoManager->SetTopVolume(pMainDetectorVolume);

    this->InitializeSubDetectors(pMainDetectorVolume, pAluminium, transparency);
    this->InitializeGaps(pMainDetectorVolume, pVacuum, transparency);

    //--- close the geometry
    pGeoManager->CloseGeometry();

    try
    {
        std::cout << "PandoraMonitoring::InitializeEve(): ";
        const char *pDisplay(::getenv("DISPLAY"));

        if (NULL == pDisplay)
        {
            std::cout << "DISPLAY environment not set" << std::endl;
        }
        else
        {
            std::cout << "DISPLAY environment set to " << pDisplay << std::endl;
        }

        TEveManager::Create();
    }
    catch (TEveException &tEveException)
    {
        std::cout << "PandoraMonitoring::InitializeEve(): Caught TEveException: " << tEveException.what() << std::endl;

        try
        {
            std::cout << "PandoraMonitoring::InitializeEve(): Attempt to release ROOT from batch mode." << std::endl;
            gROOT->SetBatch(kFALSE);
            TEveManager::Create();
        }
        catch (TEveException &tEveException)
        {
            std::cout << "PandoraMonitoring::InitializeEve(): Caught TEveException: " << tEveException.what() << std::endl;
            throw std::exception();
        }
    }

    TGeoNode *pGeoNode = gGeoManager->GetTopNode();
    TEveGeoTopNode *pEveGeoTopNode = new TEveGeoTopNode(gGeoManager, pGeoNode);
    pEveGeoTopNode->SetVisLevel(1);
    pEveGeoTopNode->GetNode()->GetVolume()->SetVisibility(kFALSE);

    gEve->AddGlobalElement(pEveGeoTopNode);

    TGLViewer *viewerGL = gEve->GetDefaultGLViewer();
    viewerGL->ColorSet().Background().SetColor(kWhite);

    gEve->Redraw3D(kTRUE);

    m_eveInitialized = true;
    m_openEveEvent = true;
}

//------------------------------------------------------------------------------------------------------------------------------------------

void PandoraMonitoring::SetEveDisplayParameters(const bool blackBackground, const bool showDetectors, const float maximumHitEnergy)
{
    InitializeEve();

    TGLViewer *viewerGL = gEve->GetDefaultGLViewer();
    viewerGL->ColorSet().Background().SetColor(blackBackground ? GetROOTColor(BLACK) : GetROOTColor(WHITE));

    gEve->GetGlobalScene()->SetRnrSelf(showDetectors);
    gEve->Redraw3D(kTRUE);

    m_maximumHitEnergy = maximumHitEnergy;
}

//------------------------------------------------------------------------------------------------------------------------------------------

void PandoraMonitoring::InitializeSubDetectors(TGeoVolume *pMainDetectorVolume, TGeoMedium *pSubDetectorMedium, Char_t transparency)
{
    typedef std::vector<std::pair<GeometryHelper::SubDetectorParameters, std::string> > SubDetectorParametersList;
    SubDetectorParametersList subDetectorParametersList;

    const GeometryHelper::SubDetectorParametersMap &subDetectorParametersMap(GeometryHelper::GetAdditionalSubDetectors());

    for (GeometryHelper::SubDetectorParametersMap::const_iterator iter = subDetectorParametersMap.begin(); iter != subDetectorParametersMap.end(); ++iter)
    {
        subDetectorParametersList.push_back(std::make_pair(iter->second, iter->first));
    }

    if (GeometryHelper::GetInDetBarrelParameters().IsInitialized())
        subDetectorParametersList.push_back(std::make_pair(GeometryHelper::GetInDetBarrelParameters(), "InDetBarrel"));

    if (GeometryHelper::GetInDetEndCapParameters().IsInitialized())
        subDetectorParametersList.push_back(std::make_pair(GeometryHelper::GetInDetEndCapParameters(), "InDetEndCap"));

    if (GeometryHelper::GetECalBarrelParameters().IsInitialized())
        subDetectorParametersList.push_back(std::make_pair(GeometryHelper::GetECalBarrelParameters(), "ECalBarrel"));

    if (GeometryHelper::GetECalEndCapParameters().IsInitialized())
        subDetectorParametersList.push_back(std::make_pair(GeometryHelper::GetECalEndCapParameters(), "ECalEndCap"));

    if (GeometryHelper::GetHCalBarrelParameters().IsInitialized())
        subDetectorParametersList.push_back(std::make_pair(GeometryHelper::GetHCalBarrelParameters(), "HCalBarrel"));

    if (GeometryHelper::GetHCalEndCapParameters().IsInitialized())
        subDetectorParametersList.push_back(std::make_pair(GeometryHelper::GetHCalEndCapParameters(), "HCalEndCap"));

    if (GeometryHelper::GetMuonBarrelParameters().IsInitialized())
        subDetectorParametersList.push_back(std::make_pair(GeometryHelper::GetMuonBarrelParameters(), "MuonBarrel"));

    if (GeometryHelper::GetMuonEndCapParameters().IsInitialized())
        subDetectorParametersList.push_back(std::make_pair(GeometryHelper::GetMuonEndCapParameters(), "MuonEndCap"));

    typedef std::set<std::string> StringSet;
    StringSet setInvisible;
    setInvisible.insert("MuonBarrel");

    if (GeometryHelper::GetMuonBarrelParameters().IsInitialized()) // set muon endcap invisible if barrel is initialized. In case of a test beam set up without barrel the muon endcap (=tail catcher) is then drawn
        setInvisible.insert("MuonEndCap");

    try
    {
        TGeoVolume *pMainTracker = NULL;
        pMainTracker = MakePolygonTube("Tracker", 0, 0, GeometryHelper::GetMainTrackerInnerRadius() * m_scalingFactor,
            GeometryHelper::GetMainTrackerOuterRadius() * m_scalingFactor, 0., 0., GeometryHelper::GetMainTrackerZExtent() * m_scalingFactor,
            pSubDetectorMedium);

        pMainTracker->SetLineColor(kGreen);
        pMainTracker->SetTransparency(transparency);
        pMainTracker->SetVisibility(kFALSE);
        pMainDetectorVolume->AddNode(pMainTracker, 0, new TGeoTranslation(0, 0, 0));
    }
    catch (StatusCodeException &)
    {
    }

    try
    {
        TGeoVolume *pCoil = NULL;
        pCoil = MakePolygonTube("Coil", 0, 0, GeometryHelper::GetCoilInnerRadius() * m_scalingFactor,
            GeometryHelper::GetCoilOuterRadius() * m_scalingFactor, 0., 0., GeometryHelper::GetCoilZExtent() * m_scalingFactor,
            pSubDetectorMedium);

        pCoil->SetLineColor(kBlue);
        pCoil->SetTransparency(transparency);
        pCoil->SetVisibility(kFALSE);
        pMainDetectorVolume->AddNode(pCoil, 0, new TGeoTranslation(0,0,0));
    }
    catch (StatusCodeException &)
    {
    }

    int col = 2;
    for (SubDetectorParametersList::const_iterator iter = subDetectorParametersList.begin(); iter != subDetectorParametersList.end(); ++iter)
    {
        bool left = true;
        for (int lr = 0; lr <= 1; ++lr)
        {
            const GeometryHelper::SubDetectorParameters &detPar = (*iter).first;

            if (left && !detPar.IsMirroredInZ())
            {
                left = false;
                continue;
            }

            const std::string name = (*iter).second;
            StringSet::iterator itSetInvisible = setInvisible.find(name);
            bool drawInvisible = (itSetInvisible != setInvisible.end() ? true : false);

            std::stringstream sstr;
            sstr << name;
            sstr << (left? "_left" : "_right");

            TGeoVolume* subDetVol = NULL;

            int sign = (left? -1 : 1);
            double zMin = detPar.GetInnerZCoordinate() * m_scalingFactor;
            double zMax = detPar.GetOuterZCoordinate() * m_scalingFactor;
            double zThick = zMax - zMin;
            zMin *= sign;
            zMax *= sign;
            double zPosition = zMin + sign * (zThick / 2.0);

            subDetVol = MakePolygonTube(sstr.str().c_str(), detPar.GetInnerSymmetryOrder(), detPar.GetOuterSymmetryOrder(),
                detPar.GetInnerRCoordinate() * m_scalingFactor, detPar.GetOuterRCoordinate() * m_scalingFactor,
                detPar.GetInnerPhiCoordinate(), detPar.GetOuterPhiCoordinate(), (zThick / 2.), pSubDetectorMedium);

            subDetVol->SetLineColor(GetROOTColor(Color(col)));
            subDetVol->SetFillColor(GetROOTColor(Color(col)));
            subDetVol->SetTransparency(transparency);

            const size_t found(name.find("subDet_"));
            if (found != std::string::npos || drawInvisible)
                subDetVol->SetVisibility(kFALSE);

            pMainDetectorVolume->AddNode(subDetVol, 0, new TGeoTranslation(0, 0, zPosition));
            left = false;
        }
        ++col;
    }
}

//------------------------------------------------------------------------------------------------------------------------------------------

void PandoraMonitoring::InitializeGaps(TGeoVolume *pMainDetectorVolume, TGeoMedium *pGapMedium, Char_t transparency)
{
    const GeometryHelper::DetectorGapList &detectorGapList(GeometryHelper::GetDetectorGapList());
    unsigned int gapCounter(0);

    for (GeometryHelper::DetectorGapList::const_iterator iter = detectorGapList.begin(), iterEnd = detectorGapList.end(); iter != iterEnd; ++iter)
    {
        std::string gapName("gap" + TypeToString(gapCounter++));

        BoxGap *pBoxGap = NULL;
        pBoxGap = dynamic_cast<BoxGap *>(*iter);

        if (NULL != pBoxGap)
        {
            TGeoShape *pGapShape = new TGeoBBox(gapName.c_str(), 0.5f * pBoxGap->m_side1.GetMagnitude() * m_scalingFactor,
                0.5f * pBoxGap->m_side2.GetMagnitude() * m_scalingFactor, 0.5f * pBoxGap->m_side3.GetMagnitude() * m_scalingFactor);

            TGeoVolume *pGapVol = new TGeoVolume(gapName.c_str(), pGapShape, pGapMedium);

            static const float pi(std::acos(-1.));
            float correction(0.f);

            try
            {
                // TODO Remove ILD-specific correction, required for endcap box gaps that do not point back to origin in xy plane.
                //      Pandora gaps are self-describing (four vectors), but this does not map cleanly to TGeoBBox class.
                //      Best solution may be to move to different root TGeoShape.
                const float vertexZ(pBoxGap->m_vertex.GetZ());
                static const float hcalEndCapInnerZ(std::fabs(GeometryHelper::GetHCalEndCapParameters().GetInnerZCoordinate()));
                correction = ((std::fabs(vertexZ) < hcalEndCapInnerZ) ? 0 : ((vertexZ > 0) ? pi / 4.f : -pi / 4.f));
            }
            catch (StatusCodeException &)
            {
            }

            const float phi(correction + std::atan2(pBoxGap->m_vertex.GetX(), pBoxGap->m_vertex.GetY()));

            const TGeoTranslation trans("trans",
                ( 0.5f * pBoxGap->m_side1.GetMagnitude() * std::cos(phi) + 0.5f * pBoxGap->m_side2.GetMagnitude() * std::sin(phi) + pBoxGap->m_vertex.GetX()) * m_scalingFactor,
                (-0.5f * pBoxGap->m_side1.GetMagnitude() * std::sin(phi) + 0.5f * pBoxGap->m_side2.GetMagnitude() * std::cos(phi) + pBoxGap->m_vertex.GetY()) * m_scalingFactor,
                ( 0.5f * (2.f * pBoxGap->m_vertex.GetZ() + pBoxGap->m_side3.GetZ()) * m_scalingFactor));

            const TGeoRotation rot("rot", -180.f * phi / pi, 0, 0);

            pGapVol->SetLineColor(1);
            pGapVol->SetFillColor(1);
            pGapVol->SetTransparency(transparency + 23);

            pMainDetectorVolume->AddNode(pGapVol, 0, new TGeoCombiTrans(trans, rot));
            continue;
        }

        ConcentricGap *pConcentricGap = NULL;
        pConcentricGap = dynamic_cast<ConcentricGap *>(*iter);

        if (NULL != pConcentricGap)
        {
            const double zMin = pConcentricGap->m_minZCoordinate * m_scalingFactor;
            const double zMax = pConcentricGap->m_maxZCoordinate * m_scalingFactor;
            const double zThick = zMax - zMin;

            TGeoVolume *pGapVol = MakePolygonTube(gapName.c_str(), pConcentricGap->m_innerSymmetryOrder, pConcentricGap->m_outerSymmetryOrder,
                pConcentricGap->m_innerRCoordinate * m_scalingFactor, pConcentricGap->m_outerRCoordinate * m_scalingFactor,
                pConcentricGap->m_innerPhiCoordinate, pConcentricGap->m_outerPhiCoordinate, (zThick / 2.), pGapMedium);

            pGapVol->SetLineColor(1);
            pGapVol->SetFillColor(1);
            pGapVol->SetTransparency(transparency + 23);

            pMainDetectorVolume->AddNode(pGapVol, 0, new TGeoTranslation(0, 0, zMin + (zThick / 2.f)));
            continue;
        }
    }
}

//------------------------------------------------------------------------------------------------------------------------------------------

void PandoraMonitoring::ViewEvent()
{
    InitializeEve();

    gEve->Redraw3D();
    this->Pause();

    TEveEventManager* current = gEve->GetCurrentEvent();
    if (current)
        current->SetRnrSelfChildren(kFALSE,kFALSE);

    m_openEveEvent = false;
    std::cout << "View done" << std::endl;
}

//------------------------------------------------------------------------------------------------------------------------------------------

TEveElement *PandoraMonitoring::VisualizeCaloHits(const OrderedCaloHitList *const pOrderedCaloHitList, std::string name,
    TEveElement *parent, Color color, int pfoId )
{
    InitializeEve();

    TEveBoxSet *hits = new TEveBoxSet(name.c_str());
    hits->Reset(TEveBoxSet::kBT_FreeBox, kTRUE, 64);
    hits->SetOwnIds(kTRUE);
    hits->SetPickable(kTRUE);
    hits->SetMainTransparency(5);
#if ROOT_VERSION_CODE >= ROOT_VERSION(5,27,02)
    hits->SetAntiFlick(kTRUE);
#endif

#if ROOT_VERSION_CODE < ROOT_VERSION(5,27,02)
    const std::string hitListName(name.empty() ? "Hits" : name);
    TEvePointSet *hitsMarkers = new TEvePointSet((hitListName+"_markers").c_str());
    hitsMarkers->SetOwnIds(kTRUE);
    hits->AddElement(hitsMarkers);
#endif 

    PandoraMonitoringApi::PdgCodeToEnergyMap pdgCodeToEnergyMap;

    PseudoLayer firstLayer = 0;
    PseudoLayer lastLayer = 0;

    firstLayer = pOrderedCaloHitList->begin()->first;

    float minRadiationLengthsFromIp = std::numeric_limits<float>::max(), maxRadiationLengthsFromIp = std::numeric_limits<float>::min();
    float minInteractionLengthsFromIp = std::numeric_limits<float>::max(), maxInteractionLengthsFromIp = std::numeric_limits<float>::min();

    float energySumElectromagnetic = 0.f;
    float energySumHadronic = 0.f;

    for (OrderedCaloHitList::const_iterator iter = pOrderedCaloHitList->begin(), iterEnd = pOrderedCaloHitList->end();
         iter != iterEnd; ++iter)
    {
        lastLayer = iter->first;

        for (CaloHitList::const_iterator caloHitIter = iter->second->begin(), caloHitIterEnd = iter->second->end();
             caloHitIter != caloHitIterEnd; ++caloHitIter)
        {
            const CaloHit *pCaloHit = (*caloHitIter);

            // Path length properties
            const float radiationLengthsFromIp(pCaloHit->GetNRadiationLengthsFromIp());

            if (radiationLengthsFromIp < minRadiationLengthsFromIp)
                minRadiationLengthsFromIp = radiationLengthsFromIp;

            if (radiationLengthsFromIp > maxRadiationLengthsFromIp)
                maxRadiationLengthsFromIp = radiationLengthsFromIp;

            const float interactionLengthsFromIp(pCaloHit->GetNInteractionLengthsFromIp());

            if (interactionLengthsFromIp < minInteractionLengthsFromIp)
                minInteractionLengthsFromIp = interactionLengthsFromIp;

            if (interactionLengthsFromIp > maxInteractionLengthsFromIp)
                maxInteractionLengthsFromIp = interactionLengthsFromIp;

            // Energy properties
            const float hitEnergy(pCaloHit->GetElectromagneticEnergy());
            energySumElectromagnetic += hitEnergy;

            const float hitEnergyHadronic(pCaloHit->GetHadronicEnergy());
            energySumHadronic += hitEnergyHadronic;

            // MC particle id
            const MCParticle *pMCParticle = NULL;
            pCaloHit->GetMCParticle(pMCParticle);

            int particleId = 0;
            if (pMCParticle)
            {
                particleId = pMCParticle->GetParticleId();
            }

            PandoraMonitoringApi::PdgCodeToEnergyMap::iterator it = pdgCodeToEnergyMap.find(particleId);

            if (pdgCodeToEnergyMap.end() == it)
            {
                pdgCodeToEnergyMap.insert(std::make_pair(particleId, hitEnergy));
            }
            else
            {
                float oldValue = it->second;
                it->second = oldValue + hitEnergy;
            }

            // Compute the corners of calohit calorimeter-cell, 8 corners x 3 dimensions
            float corners[24];
            MakeCaloHitCell(pCaloHit, corners);

            // Supply hit marker details
            const CartesianVector position = pCaloHit->GetPositionVector() * m_scalingFactor;

            Color hitColor = color;

            if (color == AUTOID)
            {
                if (pfoId == particleId)
                {
                    hitColor = GRAY;
                }
                else
                {
                    hitColor = GetColorForPdgCode(particleId);
                }
            }
 
            if (color == AUTOTYPE)
            {
                static TParticle particlePfo;
                static TParticle particleHit;

                particlePfo.SetPdgCode(pfoId);
                particleHit.SetPdgCode(particleId);
            
                TParticlePDG* pParticlePfoPDG = particlePfo.GetPDG();
                TParticlePDG* pParticleHitPDG = particleHit.GetPDG();

                hitColor = GetColorForPdgCode(particleId); // default 

                if (((pfoId == E_MINUS) || (pfoId == E_PLUS)) && ((particleId == E_MINUS) || (particleId == E_PLUS)))
                    hitColor = GRAY;

                if (((pfoId == MU_MINUS) || (pfoId == MU_PLUS)) && ((particleId == MU_MINUS) || (particleId == MU_PLUS)))
                    hitColor = GRAY;

                if ((pfoId == PHOTON) && (particleId == PHOTON))
                    hitColor = GRAY;

                if ((std::abs(pfoId) >= 100) && (std::abs(particleId) >= 100))
                {
                    if (pParticlePfoPDG->Charge() == pParticleHitPDG->Charge())
                        hitColor = GRAY;
                }
            }

#if ROOT_VERSION_CODE < ROOT_VERSION(5,27,02)
            if (color < AUTOID)
            {
                const float markerSize(0.1);
                hitsMarkers->SetNextPoint(position.GetX(), position.GetY(), position.GetZ());
                hitsMarkers->SetMarkerColor(GetROOTColor(hitColor));
                hitsMarkers->SetMarkerSize(markerSize);
                hitsMarkers->SetMarkerStyle(4);
            }
#endif
            // Add calorimeter-cell for calo-hit
            hits->AddBox(corners);

            char transparency = static_cast<char>(255 - 255.f * (hitEnergy / m_maximumHitEnergy));

            if (hitEnergy > m_maximumHitEnergy)
                transparency = 0;

            hits->DigitColor(GetROOTColor(hitColor),transparency);
        }
    }

    // Build information string
    std::stringstream sstr, sstrName;

    if (!name.empty())
        sstr << name << "\n";

    sstr << "--- calo-hits"
         << "\nEem=" << energySumElectromagnetic
         << "\nEhad=" << energySumHadronic
         << "\nfirst pseudo-layer=" << firstLayer
         << "\nlast  pseudo-layer=" << lastLayer
         << "\nmin radLenFromIP=" << minRadiationLengthsFromIp
         << "\nmax radLenFromIP=" << maxRadiationLengthsFromIp
         << "\nmin intLenFromIP=" << minInteractionLengthsFromIp
         << "\nmax intLenFromIP=" << maxInteractionLengthsFromIp;

    sstrName << "calohits"
             << "/Eem=" << energySumElectromagnetic
             << "/Ehad=" << energySumHadronic
             << "/first pseudo-layer=" << firstLayer
             << "/last  pseudo-layer=" << lastLayer
             << "/min radLenFromIP=" << minRadiationLengthsFromIp
             << "/max radLenFromIP=" << maxRadiationLengthsFromIp
             << "/min intLenFromIP=" << minInteractionLengthsFromIp
             << "/max intLenFromIP=" << maxInteractionLengthsFromIp;
    
    for (PandoraMonitoringApi::PdgCodeToEnergyMap::const_iterator it = pdgCodeToEnergyMap.begin(), itEnd = pdgCodeToEnergyMap.end();
        it != itEnd; ++it)
    {
        const int mcPDG(it->first);
        const float energy(it->second);

        if (0 == mcPDG)
        {
            sstr << "\nCaloHits w/o MC particle = " << energy << " GeV";
            sstrName << "/CaloHits wo MC particle = " << energy << " GeV";
        }
        else
        {
            sstr << "\nfrom MC with PDG " << mcPDG << " = " << energy << " GeV";
            sstrName << "/from MC with PDG " << mcPDG << " = " << energy << " GeV";
        }
    }

    hits->SetElementName(sstrName.str().c_str());
    hits->SetElementTitle(sstr.str().c_str());

    if (parent)
    {
        parent->AddElement(hits);
    }
    else
    {
        gEve->AddElement(hits);
        gEve->Redraw3D();
    }

    return hits;
}

//------------------------------------------------------------------------------------------------------------------------------------------

TEveElement *PandoraMonitoring::VisualizeMCParticles(const MCParticleList *const pMCParticleList, std::string name,
    TEveElement *parent, Color color, const PandoraMonitoringApi::PdgCodeToEnergyMap *pParticleSuppressionMap)
{
    MCParticleVector mcParticleVector(pMCParticleList->begin(), pMCParticleList->end());
    std::sort(mcParticleVector.begin(), mcParticleVector.end(), MCParticle::SortByEnergy);

    InitializeEve();

    TEveTrackList *pTEveTrackList = new TEveTrackList();
    const std::string mcParticleListTitle(name.empty() ? "MCParticles" : name);

    std::string mcParticleListName(mcParticleListTitle);
    std::replace_if(mcParticleListName.begin(), mcParticleListName.end(), std::bind2nd(std::equal_to<char>(),'\n'), '/');

    pTEveTrackList->SetElementNameTitle( mcParticleListName.c_str(), mcParticleListTitle.c_str() );
    pTEveTrackList->SetMainColor(GetROOTColor(TEAL));

    // Initialize magnetic field for particle propagation, note strange ALICE charge sign convention,
    // see http://root.cern.ch/phpBB3/viewtopic.php?f=3&t=9456&p=40325&hilit=teve+histogram#p40325
    TEveTrackPropagator *pTEveTrackPropagator = pTEveTrackList->GetPropagator();
    pTEveTrackPropagator->SetMagFieldObj(new TEveMagFieldConst(0., 0., -GeometryHelper::GetBField(CartesianVector(0., 0., 0.))));
    pTEveTrackPropagator->SetMaxOrbs(5);

    try {pTEveTrackPropagator->SetMaxR(GeometryHelper::GetHCalBarrelParameters().GetOuterRCoordinate() * m_scalingFactor);}
    catch (StatusCodeException &) {}

    try {pTEveTrackPropagator->SetMaxZ(std::fabs(GeometryHelper::GetHCalEndCapParameters().GetOuterZCoordinate()) * m_scalingFactor);}
    catch (StatusCodeException &) {}

    for (MCParticleVector::const_iterator mcParticleIter = mcParticleVector.begin(), mcParticleIterEnd = mcParticleVector.end();
         mcParticleIter != mcParticleIterEnd; ++mcParticleIter)
    { 
        MCParticle *pPandoraMCParticle = (*mcParticleIter);

        if (!pPandoraMCParticle->IsInitialized())
            continue;

        // Get mc particle position and momentum
        const CartesianVector &momentum(pPandoraMCParticle->GetMomentum());
        const float energy(pPandoraMCParticle->GetEnergy());

        const CartesianVector position(pPandoraMCParticle->GetVertex() * m_scalingFactor);
        const CartesianVector positionAtEnd(pPandoraMCParticle->GetEndpoint() * m_scalingFactor);

        // Does particle pass suppression conditions?
        const int particleId = pPandoraMCParticle->GetParticleId();
        const float innerRadius = pPandoraMCParticle->GetInnerRadius();
        const float outerRadius = pPandoraMCParticle->GetOuterRadius();
        int charge = 0;

        if (NULL != pParticleSuppressionMap)
        {
            PandoraMonitoringApi::PdgCodeToEnergyMap::const_iterator itSuppPtcl = pParticleSuppressionMap->find(particleId);

            if ((itSuppPtcl != pParticleSuppressionMap->end()) && (itSuppPtcl->second > energy))
            {
                continue;
            }
        }

        // Create particle path
        TEveMCTrack *pTEveRecTrack = new TEveMCTrack();

        pTEveRecTrack->SetProductionVertex(position.GetX(), position.GetY(), position.GetZ(), 0.f);
        pTEveRecTrack->SetMomentum(momentum.GetX(), momentum.GetY(), momentum.GetZ(), energy);
        pTEveRecTrack->SetPdgCode(particleId);

        // If known PDG code
        if (pTEveRecTrack->GetPDG())
            charge = ((int)pTEveRecTrack->GetPDG()->Charge()) / 3;

        // Color assignment
        Color mcParticleColor = color;

        if (color >= AUTO)
        {
            mcParticleColor = CYAN;

            if (charge > 0)
            {
                mcParticleColor = LIGHTRED;
            }
            else if(charge < 0)
            {
                mcParticleColor = LIGHTGREEN;
            }
        }

        // Build information string
        std::stringstream sstr;
        std::stringstream sstrName;

        if (!name.empty())
            sstr << name << "\n";

        sstr << "--- MC particle"
             << "\np=" << momentum.GetMagnitude()
             << "\nE=" << energy
             << "\nCharge=" << charge
             << "\nPDG=" << particleId
             << "\nr_inner=" << innerRadius
             << "\nr_outer=" << outerRadius;

        sstrName << "MC"
                 << "/PDG=" << particleId
                 << "/p=" << momentum.GetMagnitude()
                 << "/E=" << energy
                 << "/Charge=" << charge
                 << "/r_inner=" << innerRadius
                 << "/r_outer=" << outerRadius;

        TEveTrack *pTEveTrack = new TEveTrack(pTEveRecTrack, pTEveTrackPropagator);
        pTEveTrack->SetName(sstrName.str().c_str());
        pTEveTrack->SetTitle(sstr.str().c_str());
        pTEveTrack->SetLineColor(GetROOTColor(mcParticleColor));
        pTEveTrack->SetLineWidth(1);
        pTEveTrack->SetLineStyle(2);
        pTEveTrack->SetPickable(kTRUE);

        // Create mark at end position
        TEvePathMark endPositionMark(TEvePathMark::kDecay);
        endPositionMark.fV.Set(positionAtEnd.GetX(), positionAtEnd.GetY(), positionAtEnd.GetZ());
        pTEveTrack->AddPathMark(endPositionMark);

        pTEveTrackList->AddElement(pTEveTrack);
        pTEveTrack->MakeTrack();
    }

    if (parent)
    {
        parent->AddElement(pTEveTrackList);
    }
    else
    {
        gEve->AddElement(pTEveTrackList);
        gEve->Redraw3D();
    }

    return pTEveTrackList;
}

//------------------------------------------------------------------------------------------------------------------------------------------

TEveElement *PandoraMonitoring::VisualizeTracks(const TrackList *const pTrackList, std::string name, TEveElement *parent, Color color)
{
    TrackVector trackVector(pTrackList->begin(), pTrackList->end());
    std::sort(trackVector.begin(), trackVector.end(), Track::SortByMomentum);

    InitializeEve();

    TEveTrackList *pTEveTrackList = new TEveTrackList();
    const std::string trackListTitle(name.empty() ? "Tracks" : name);

    const std::string starter("--- ");
    std::string trackListName(trackListTitle);
    if (trackListName.find(starter) != std::string::npos)
        trackListName.replace(trackListName.find(starter), starter.length(), "Tracks//");
    std::replace_if(trackListName.begin(), trackListName.end(), std::bind2nd(std::equal_to<char>(),'\n'), '/');

    pTEveTrackList->SetElementNameTitle( trackListName.c_str(), trackListTitle.c_str() );
    pTEveTrackList->SetMainColor(GetROOTColor(TEAL));

    // Initialize magnetic field for particle propagation, note strange ALICE charge sign convention,
    // see http://root.cern.ch/phpBB3/viewtopic.php?f=3&t=9456&p=40325&hilit=teve+histogram#p40325
    TEveTrackPropagator *pTEveTrackPropagator = pTEveTrackList->GetPropagator();
    pTEveTrackPropagator->SetMagFieldObj(new TEveMagFieldConst(0., 0., -GeometryHelper::GetBField(CartesianVector(0., 0., 0.))));
    pTEveTrackPropagator->SetMaxOrbs(5);

    try {pTEveTrackPropagator->SetMaxR(GeometryHelper::GetECalBarrelParameters().GetOuterRCoordinate() * m_scalingFactor);}
    catch (StatusCodeException &) {}

    try {pTEveTrackPropagator->SetMaxZ(std::fabs(GeometryHelper::GetECalEndCapParameters().GetOuterZCoordinate()) * m_scalingFactor);}
    catch (StatusCodeException &) {}

    for (TrackVector::const_iterator trackIter = trackVector.begin(), trackIterEnd = trackVector.end();
        trackIter != trackIterEnd; ++trackIter)
    { 
        Track *pPandoraTrack = (*trackIter);

        // Extract pandora track states
        const TrackState &trackState(pPandoraTrack->GetTrackStateAtStart());
        const CartesianVector &momentum(trackState.GetMomentum());
        const CartesianVector position(trackState.GetPosition() * m_scalingFactor);

        const TrackState &trackStateAtEnd(pPandoraTrack->GetTrackStateAtEnd());
        const CartesianVector &momentumAtEnd(trackStateAtEnd.GetMomentum());
        const CartesianVector positionAtEnd(trackStateAtEnd.GetPosition() * m_scalingFactor);

        const TrackState &trackStateAtCalorimeter(pPandoraTrack->GetTrackStateAtCalorimeter());
        const CartesianVector positionAtCalorimeter(trackStateAtCalorimeter.GetPosition() * m_scalingFactor);

        // Color assignment
        const int charge(pPandoraTrack->GetCharge());

        Color trackColor = color;
        if (color >= AUTO)
        {
            trackColor = AZURE;

            if (charge > 0)
            {
                trackColor = RED;
            }
            else if(charge < 0)
            {
                trackColor = GREEN;
            }
        }

        const MCParticle* pMCParticle = NULL;
        pPandoraTrack->GetMCParticle(pMCParticle);

        // Build information string
        std::stringstream sstr, sstrName;

        if (!name.empty())
            sstr << name << "\n";

        sstr << "--- track"
             << "\np=" << momentum.GetMagnitude()
             << "\nCharge=" << charge
             << "\nPDG=" << pPandoraTrack->GetParticleId();

        sstrName << "track"
                 << "/p=" << momentum.GetMagnitude()
                 << "/Charge=" << charge
                 << "/PDG=" << pPandoraTrack->GetParticleId();

        if (pMCParticle)
        {
            int mcPdg = pMCParticle->GetParticleId();
            sstr << "\nPDG_MC=" << mcPdg;
            sstrName << "/PDG_MC=" << mcPdg;
        }

        // Create track path
        TEveRecTrack *pTEveRecTrack = new TEveRecTrack();
        pTEveRecTrack->fV.Set(position.GetX(), position.GetY(), position.GetZ());
        pTEveRecTrack->fP.Set(momentum.GetX(), momentum.GetY(), momentum.GetZ());
        pTEveRecTrack->fSign = charge;

        TEveTrack *pTEveTrack = new TEveTrack(pTEveRecTrack, pTEveTrackPropagator);
        pTEveTrack->SetName(sstrName.str().c_str());
        pTEveTrack->SetTitle(sstr.str().c_str());
        pTEveTrack->SetLineColor(GetROOTColor(trackColor));
        pTEveTrack->SetLineWidth(1);
        pTEveTrack->SetPickable(kTRUE);

        // Create mark at track end
        TEvePathMark endPositionMark(TEvePathMark::kReference);
        endPositionMark.fV.Set(positionAtEnd.GetX(), positionAtEnd.GetY(), positionAtEnd.GetZ());
        endPositionMark.fP.Set(momentumAtEnd.GetX(), momentumAtEnd.GetY(), momentumAtEnd.GetZ());
        pTEveTrack->AddPathMark(endPositionMark);

        // Create mark at track projection to calorimeter
        TEvePathMark calorimeterPositionMark(TEvePathMark::kDecay);
        calorimeterPositionMark.fV.Set(positionAtCalorimeter.GetX(), positionAtCalorimeter.GetY(), positionAtCalorimeter.GetZ());
        pTEveTrack->AddPathMark(calorimeterPositionMark);

        pTEveTrackList->AddElement(pTEveTrack);
        pTEveTrack->MakeTrack();
    }

    if (parent)
    {
        parent->AddElement(pTEveTrackList);
    }
    else
    {
        gEve->AddElement(pTEveTrackList);
        gEve->Redraw3D();
    }

    return pTEveTrackList;
}

//------------------------------------------------------------------------------------------------------------------------------------------

TEveElement *PandoraMonitoring::VisualizeParticleFlowObjects(const ParticleFlowObjectList *const pPfoList, std::string name,
    TEveElement *parent, Color color, bool showAssociatedTracks, bool showFit)
{
    ParticleFlowObjectVector pfoVector(pPfoList->begin(), pPfoList->end());
    std::sort(pfoVector.begin(), pfoVector.end(), ParticleFlowObject::SortByEnergy);

    InitializeEve();

    TEveElement *pPfoVectorElement = new TEveElementList();
    const std::string pfoListTitle(name.empty() ? "Pfos" : name);

    std::string pfoListName(pfoListTitle);
    std::replace_if(pfoListName.begin(), pfoListName.end(), std::bind2nd(std::equal_to<char>(),'\n'), '/');

    pPfoVectorElement->SetElementNameTitle(pfoListName.c_str(), pfoListTitle.c_str());

    for (ParticleFlowObjectVector::const_iterator pfoIter = pfoVector.begin(), pfoIterEnd = pfoVector.end();
        pfoIter != pfoIterEnd; ++pfoIter)
    { 
        ParticleFlowObject *pPfo = (*pfoIter);

        // Build information string
        std::stringstream sstr;
        sstr << "--- PFO"
             << "\nE=" << pPfo->GetEnergy() 
             << "\nm=" << pPfo->GetMass()
             << "\nPDG=" << pPfo->GetParticleId();

        // Default color assignment
        Color pfoColor = color;

        if (color >= AUTO)
        {
            pfoColor = GetColorForPdgCode(pPfo->GetParticleId());
        }

        // show clusters and tracks
        const ClusterList &clusterList(pPfo->GetClusterList());
        const TrackList &trackList(pPfo->GetTrackList());

        if (clusterList.empty())
        {
            VisualizeTracks(&trackList, sstr.str().c_str(), pPfoVectorElement, pfoColor);
        }
        else
        {
            if ((color == AUTOID) || (color == AUTOTYPE))
                pfoColor = color;

            if (color == AUTOITER)
            {
                static int colorIter = RED;
                if (colorIter >= AUTO)
                    colorIter = RED;

                pfoColor = Color(colorIter);
                colorIter++;
            }

            VisualizeClusters(&clusterList, sstr.str().c_str(), pPfoVectorElement, pfoColor, showAssociatedTracks, showFit, pPfo->GetParticleId());
        }
    }

    if (parent)
    {
        parent->AddElement(pPfoVectorElement);
    }
    else
    {
        gEve->AddElement(pPfoVectorElement);
        gEve->Redraw3D();
    }

    return pPfoVectorElement;
}

//------------------------------------------------------------------------------------------------------------------------------------------

TEveElement *PandoraMonitoring::VisualizeClusters(const ClusterList *const pClusterList, std::string name, TEveElement *parent,
    Color color, bool showAssociatedTracks, bool showFit, int pfoId)
{
    ClusterVector clusterVector(pClusterList->begin(), pClusterList->end());
    std::sort(clusterVector.begin(), clusterVector.end(), Cluster::SortByHadronicEnergy);

    InitializeEve();

    TEveElement *pClusterVectorElement = new TEveElementList();
    const std::string clusterListTitle(name.empty() ? "Clusters" : name);

    const std::string starter("--- ");
    std::string clusterListName(clusterListTitle);
    if (clusterListName.find(starter) != std::string::npos)
        clusterListName.replace(clusterListName.find(starter), starter.length(), "");
    std::replace_if(clusterListName.begin(), clusterListName.end(), std::bind2nd(std::equal_to<char>(),'\n'), '/');

    pClusterVectorElement->SetElementNameTitle( clusterListName.c_str(), clusterListTitle.c_str());

    for (ClusterVector::const_iterator clusterIter = clusterVector.begin(), clusterIterEnd = clusterVector.end();
        clusterIter != clusterIterEnd; ++clusterIter)
    {
        Cluster *pCluster = (*clusterIter);

        if (pCluster->GetNCaloHits() == 0)
            continue;

        // Color assignment
        Color clusterColor = color;

        if (color == AUTO)
        {
            const TrackList &trackList(pCluster->GetAssociatedTrackList());
            bool clusterHasTracks = !(trackList.empty());
            bool clusterIsPhoton = false;

            try {clusterIsPhoton = pCluster->IsPhotonFast();}
            catch (StatusCodeException &) {}

            if (clusterIsPhoton)
            {
                clusterColor = DARKYELLOW;
            }
            else if (clusterHasTracks)
            {
                clusterColor = MAGENTA;
            }
            else
            {
                clusterColor = LIGHTBLUE;
            }
        }

        if (color == AUTOITER)
        {
            static int colorIter = RED;
            if (colorIter >= AUTO)
                colorIter = RED;

            clusterColor = Color(colorIter);
            colorIter++;
        }

        // Build information string
        std::stringstream sstr, sstrName;

        if (!name.empty())
            sstr << name << "\n";

        sstr << "--- cluster\nEem(corr)=" << pCluster->GetElectromagneticEnergy() 
             << "\nEhad(corr)=" << pCluster->GetHadronicEnergy() 
             << "\nNHits=" << pCluster->GetNCaloHits();

        sstrName << "cluster/Eem(corr)=" << pCluster->GetElectromagneticEnergy() 
                 << "/Ehad(corr)=" << pCluster->GetHadronicEnergy() 
                 << "/NHits=" << pCluster->GetNCaloHits();

        // Display constituent calo hits
        const OrderedCaloHitList &orderedCaloHitList(pCluster->GetOrderedCaloHitList());

        Color caloHitColor = clusterColor;

        if ((color == AUTOID) || (color == AUTOTYPE) )
            caloHitColor = color;

        TEveElement *pCaloHitsElement = VisualizeCaloHits(&orderedCaloHitList, sstr.str().c_str(), pClusterVectorElement, caloHitColor, pfoId);

        const ClusterHelper::ClusterFitResult &fit = pCluster->GetFitToAllHitsResult();

        if (showFit && fit.IsFitSuccessful())
        {
            const CartesianVector intercept = fit.GetIntercept() * m_scalingFactor;
            CartesianVector direction = fit.GetDirection() * m_scalingFactor;

            const double length = 100;
            CartesianVector displacedStart = intercept - (direction * (length / 2));
            direction *= length;

            TEveArrow *pClusterArrow = new TEveArrow(direction.GetX(), direction.GetY(), direction.GetZ(), displacedStart.GetX(),
                displacedStart.GetY(), displacedStart.GetZ());

            pClusterArrow->SetConeR(0.03);
            pClusterArrow->SetConeL(0.2);
            pClusterArrow->SetMainColor(GetROOTColor(clusterColor));
            pClusterArrow->SetPickable(kTRUE);
            pClusterArrow->SetElementNameTitle(sstr.str().c_str(), sstrName.str().c_str());

            pCaloHitsElement->AddElement(pClusterArrow);
        }

        // Show tracks associated with clusters
        if (showAssociatedTracks)
        {
            const TrackList &trackList(pCluster->GetAssociatedTrackList());

            if (!trackList.empty())
            {
                VisualizeTracks(&trackList, sstr.str().c_str(), pCaloHitsElement, clusterColor);
            }
        }
    }

    if (parent)
    {
        parent->AddElement(pClusterVectorElement);
    }
    else
    {
        gEve->AddElement(pClusterVectorElement);
        gEve->Redraw3D();
    }

    return pClusterVectorElement;
}

//------------------------------------------------------------------------------------------------------------------------------------------

void PandoraMonitoring::MakeCaloHitCell(const CaloHit *const pCaloHit, float corners[24])
{
    CartesianVector dirU((ENDCAP == pCaloHit->GetDetectorRegion()) ? CartesianVector(0, 1, 0) : CartesianVector(0, 0, 1));
    CartesianVector normal(pCaloHit->GetCellNormalVector());

    CartesianVector dirV(normal.GetCrossProduct(dirU));
    const float magnitudeV(dirV.GetMagnitude());

    const CartesianVector position(pCaloHit->GetPositionVector() * m_scalingFactor);
    float u2(pCaloHit->GetCellSizeU() * m_scalingFactor / 2.0);
    float v2(pCaloHit->GetCellSizeV() * m_scalingFactor / 2.0);
    float t2(pCaloHit->GetCellThickness() * m_scalingFactor / 2.0);

    if (magnitudeV < 0.00001)
    {
        const DetectorRegion dr(pCaloHit->GetDetectorRegion());

        std::string detectorRegion((dr == ENDCAP ? "ENDCAP" : (dr == BARREL ? "BARREL" : "UNKNOWN")));

        std::cout << "PandoraMonitoring::MakeCaloHitCell, ERROR: direction vectors U and V are parallel. "
                  << "Normal(" << normal.GetX() << ", " << normal.GetY() << ", " << normal.GetZ() << ") "
                  << "U(" << dirU.GetX() << ", " << dirU.GetY() << ", " << dirU.GetZ() << ") "
                  << "V(" << dirV.GetX() << ", " << dirV.GetY() << ", " << dirV.GetZ() << ") "
                  << " in Detector-region: " << detectorRegion << "  caloHit-coordinates: "
                  << position.GetX() << ", " << position.GetY() << ", " << position.GetZ() << std::endl;

        normal.SetValues(1, 0, 0);
        dirU.SetValues(0, 1, 0);
        dirV.SetValues(0, 0, 1);

        const float size = 0.1;
        u2 = size * m_scalingFactor / 2.0;
        v2 = size * m_scalingFactor / 2.0;
        t2 = size * m_scalingFactor / 2.0;
    }
    else
    {
        dirV *= 1. / magnitudeV;
    }

    dirU *= u2;
    dirV *= v2;
    normal *= t2;

    // Compute all 8 corners for the box
    CartesianVector *pCornerVectors[8];

    pCornerVectors[0] = new CartesianVector(position - dirU - dirV - normal);
    pCornerVectors[1] = new CartesianVector(position + dirU - dirV - normal);
    pCornerVectors[2] = new CartesianVector(position + dirU + dirV - normal);
    pCornerVectors[3] = new CartesianVector(position - dirU + dirV - normal);
    pCornerVectors[4] = new CartesianVector(position - dirU - dirV + normal);
    pCornerVectors[5] = new CartesianVector(position + dirU - dirV + normal);
    pCornerVectors[6] = new CartesianVector(position + dirU + dirV + normal);
    pCornerVectors[7] = new CartesianVector(position - dirU + dirV + normal);

    // Assign corner positions to float array
    corners[0] = pCornerVectors[0]->GetX();
    corners[1] = pCornerVectors[0]->GetY();
    corners[2] = pCornerVectors[0]->GetZ();

    corners[3] = pCornerVectors[1]->GetX();
    corners[4] = pCornerVectors[1]->GetY();
    corners[5] = pCornerVectors[1]->GetZ();

    corners[6] = pCornerVectors[2]->GetX();
    corners[7] = pCornerVectors[2]->GetY();
    corners[8] = pCornerVectors[2]->GetZ();

    corners[9] = pCornerVectors[3]->GetX();
    corners[10] = pCornerVectors[3]->GetY();
    corners[11] = pCornerVectors[3]->GetZ();

    corners[12] = pCornerVectors[4]->GetX();
    corners[13] = pCornerVectors[4]->GetY();
    corners[14] = pCornerVectors[4]->GetZ();

    corners[15] = pCornerVectors[5]->GetX();
    corners[16] = pCornerVectors[5]->GetY();
    corners[17] = pCornerVectors[5]->GetZ();

    corners[18] = pCornerVectors[6]->GetX();
    corners[19] = pCornerVectors[6]->GetY();
    corners[20] = pCornerVectors[6]->GetZ();

    corners[21] = pCornerVectors[7]->GetX();
    corners[22] = pCornerVectors[7]->GetY();
    corners[23] = pCornerVectors[7]->GetZ();

    for (unsigned int i = 0; i < 8; ++i)
        delete pCornerVectors[i];
}

//------------------------------------------------------------------------------------------------------------------------------------------

TGeoVolume* PandoraMonitoring::MakePolygonTube(std::string name, int innerSymmetryOrder, int outerSymmetryOrder, double innerClosestDistanceToIp,
    double outerClosestDistanceToIp, double innerPhi0, double outerPhi0, double halfLength, TGeoMedium* medium)
{
    TGeoShape *pInnerTGeoShape = MakePolygonTube(innerSymmetryOrder, innerClosestDistanceToIp, innerPhi0, halfLength + 2);
    TGeoShape *pOuterTGeoShape = MakePolygonTube(outerSymmetryOrder, outerClosestDistanceToIp, outerPhi0, halfLength);

    std::string nameInner = (name + "_I");
    std::string nameOuter = (name + "_O");

    pInnerTGeoShape->SetName(nameInner.c_str());
    pOuterTGeoShape->SetName(nameOuter.c_str());

    std::string formula = nameOuter + "-" + nameInner;

    TGeoCompositeShape *pTGeoCompositeShape = new TGeoCompositeShape((name + "_shape").c_str(), formula.c_str());
    TGeoVolume *pTGeoVolume = new TGeoVolume(name.c_str(), pTGeoCompositeShape, medium);

    return pTGeoVolume;
}

//------------------------------------------------------------------------------------------------------------------------------------------

TGeoShape *PandoraMonitoring::MakePolygonTube(int symmetryOrder, double closestDistanceToIp, double phi, double halfLength)
{
    if (symmetryOrder <= 2)
    {
        TGeoShape *pTGeoShape = new TGeoTube(0, closestDistanceToIp, halfLength);
        return pTGeoShape;
    }

    DoublePairVector vertices;
    ComputePolygonCorners(symmetryOrder, closestDistanceToIp, phi, vertices);

    const int nvertices(vertices.size());
    double *x = new double[nvertices];
    double *y = new double[nvertices];

    int index = 0;
    for (DoublePairVector::iterator itCoord = vertices.begin(), itCoordEnd = vertices.end(); itCoord != itCoordEnd; ++itCoord)
    {
        x[index] = (*itCoord).first;
        y[index] = (*itCoord).second;
        ++index;
    }

    TGeoXtru *pTGeoXtru = new TGeoXtru(2);

    pTGeoXtru->DefinePolygon(nvertices,x,y);
    double z0 = -halfLength, x0 = 0, y0 = 0;
    double z1 = halfLength, x1 = 0, y1 = 0;

    double scale0 = 1.0;
    pTGeoXtru->DefineSection(0, z0, x0, y0, scale0); // Z position, offset and scale for first section
    pTGeoXtru->DefineSection(1, z1, x1, y1, scale0); // -''- go forward

    delete[] x;
    delete[] y;

    return pTGeoXtru;
}

//------------------------------------------------------------------------------------------------------------------------------------------

void PandoraMonitoring::ComputePolygonCorners(int symmetryOrder, double closestDistanceToIp, double phi0, DoublePairVector &coordinates)
{
    if (symmetryOrder > 2)
    {
        static const double pi(std::acos(-1.));
        const double x0(-1. * closestDistanceToIp * tan(pi / double(symmetryOrder)));
        const double y0(closestDistanceToIp);

        for (int i = 0; i < symmetryOrder; ++i)
        {
            double theta = 0.f; 
            theta = phi0 + (2 * pi * double(i) / double(symmetryOrder));

            double x = x0 * cos(theta) + y0 * sin(theta);
            double y = y0 * cos(theta) - x0 * sin(theta);
            coordinates.push_back(std::pair<double, double>(x, y));
        }
    }
}

} // namespace pandora_monitoring
