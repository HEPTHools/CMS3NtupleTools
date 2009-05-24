// -*- C++ -*-
//
// Package:    VertexMaker
// Class:      VertexMaker
// 
/**\class VertexMaker VertexMaker.cc CMS2/VertexMaker/src/VertexMaker.cc

 Description: <one line class summary>

 Implementation:
     <Notes on implementation>
*/
//
//
//

// system include files
#include <memory>

// user include files
#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/Framework/interface/EDProducer.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"

#include "CMS2/NtupleMaker/interface/VertexMaker.h"

#include "DataFormats/VertexReco/interface/VertexFwd.h"
#include "DataFormats/VertexReco/interface/Vertex.h"
#include "DataFormats/Math/interface/Point3D.h"

typedef math::XYZPoint Point;

//
// class decleration
//

//
// constructors and destructor
//
VertexMaker::VertexMaker(const edm::ParameterSet& iConfig)
{

     // counter of vertexs in the event
     produces<unsigned int>       ("evtnvtxs"              ).setBranchAlias("evt_nvtxs"              );

     // position and position error
     produces<std::vector<Point> >("vtxsposition"          ).setBranchAlias("vtxs_position"          );
     produces<std::vector<float> >("vtxsxError"            ).setBranchAlias("vtxs_xError"            );
     produces<std::vector<float> >("vtxsyError"            ).setBranchAlias("vtxs_yError"            );
     produces<std::vector<float> >("vtxszError"            ).setBranchAlias("vtxs_zError"            );

     // chi2 and ndof. Tracks apparently can contribute with a weight so
     // ndof may be non integral
     produces<std::vector<float> >("vtxschi2"              ).setBranchAlias("vtxs_chi2"              );
     produces<std::vector<float> >("vtxsndof"              ).setBranchAlias("vtxs_ndof"              );

     // track / general information
     produces<std::vector<int>   >("vtxshasRefittedTracks" ).setBranchAlias("vtxs_hasRefittedTracks" );
     produces<std::vector<int>   >("vtxsisFake"            ).setBranchAlias("vtxs_isFake"            );
     produces<std::vector<int>   >("vtxsisValid"           ).setBranchAlias("vtxs_isValid"           );
     produces<std::vector<int>   >("vtxstracksSize"        ).setBranchAlias("vtxs_tracksSize"        );

     // vertex collection input tag
     primaryVertexInputTag_ = iConfig.getParameter<edm::InputTag>("primaryVertexInputTag");

}

void VertexMaker::produce(edm::Event& iEvent, const edm::EventSetup& iSetup)
{

     // get the primary vertices
     edm::Handle<reco::VertexCollection> vertexHandle;
     try {
        iEvent.getByLabel(primaryVertexInputTag_, vertexHandle);
     }
     catch ( cms::Exception& ex ) {
        edm::LogError("VertexMakerError") << "Error! can't get the primary vertex";
     }
     const reco::VertexCollection *vertexCollection = vertexHandle.product();

     std::auto_ptr<unsigned int>        evt_nvtxs                     (new unsigned int         );
     std::auto_ptr<std::vector<Point> > vector_vtxs_position          (new std::vector<Point>   );
     std::auto_ptr<std::vector<float> > vector_vtxs_xError            (new std::vector<float>   );
     std::auto_ptr<std::vector<float> > vector_vtxs_yError            (new std::vector<float>   );
     std::auto_ptr<std::vector<float> > vector_vtxs_zError            (new std::vector<float>   );
     std::auto_ptr<std::vector<float> > vector_vtxs_chi2              (new std::vector<float>   );
     std::auto_ptr<std::vector<float> > vector_vtxs_ndof              (new std::vector<float>   );
     std::auto_ptr<std::vector<int>   > vector_vtxs_hasRefittedTracks (new std::vector<int>     );
     std::auto_ptr<std::vector<int>   > vector_vtxs_isFake            (new std::vector<int>     );
     std::auto_ptr<std::vector<int>   > vector_vtxs_isValid           (new std::vector<int>     );
     std::auto_ptr<std::vector<int>   > vector_vtxs_tracksSize        (new std::vector<int>     );
     
     *evt_nvtxs = vertexCollection->size();
     for (reco::VertexCollection::const_iterator vtx = vertexCollection->begin();
		vtx != vertexCollection->end(); ++vtx)
     {
	     vector_vtxs_position->push_back( vtx->position() );
	     vector_vtxs_xError->push_back( vtx->xError() );
             vector_vtxs_yError->push_back( vtx->yError() );
             vector_vtxs_zError->push_back( vtx->zError() );
             vector_vtxs_chi2->push_back( vtx->chi2() );
             vector_vtxs_ndof->push_back( vtx->ndof() );
             vector_vtxs_hasRefittedTracks->push_back( vtx->hasRefittedTracks() );
             vector_vtxs_isFake->push_back( vtx->isFake() );
             vector_vtxs_isValid->push_back( vtx->isValid() );
             vector_vtxs_tracksSize->push_back( vtx->tracksSize() );

     } // end loop on vertexs

     // store into the event
     iEvent.put(evt_nvtxs,                     "evtnvtxs"              );
     iEvent.put(vector_vtxs_position,          "vtxsposition"          );
     iEvent.put(vector_vtxs_xError,            "vtxsxError"            );
     iEvent.put(vector_vtxs_yError,            "vtxsyError"            );
     iEvent.put(vector_vtxs_zError,            "vtxszError"            );
     iEvent.put(vector_vtxs_chi2,              "vtxschi2"              );
     iEvent.put(vector_vtxs_ndof,              "vtxsndof"              );
     iEvent.put(vector_vtxs_hasRefittedTracks, "vtxshasRefittedTracks" );
     iEvent.put(vector_vtxs_isFake,            "vtxsisFake"            );
     iEvent.put(vector_vtxs_isValid,           "vtxsisValid"           );
     iEvent.put(vector_vtxs_tracksSize,        "vtxstracksSize"        );

}

// ------------ method called once each job just before starting event loop  ------------
void 
VertexMaker::beginJob(const edm::EventSetup&)
{
}

// ------------ method called once each job just after ending the event loop  ------------
void 
VertexMaker::endJob() 
{
}

//define this as a plug-in
DEFINE_FWK_MODULE(VertexMaker);

