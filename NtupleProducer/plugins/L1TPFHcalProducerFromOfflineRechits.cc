#include "DataFormats/Common/interface/Handle.h"
#include "DataFormats/HcalRecHit/interface/HcalRecHitCollections.h"
#include "FWCore/Framework/interface/ESHandle.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/EventSetup.h"
#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/Framework/interface/stream/EDProducer.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "Geometry/CaloGeometry/interface/CaloGeometry.h"
#include "Geometry/Records/interface/CaloGeometryRecord.h"
#include "L1Trigger/L1TCalorimeter/interface/CaloTools.h"
#include "DataFormats/Math/interface/deltaPhi.h"

#include "FastPUPPI/NtupleProducer/interface/L1TPFUtils.h"
#include "FastPUPPI/NtupleProducer/interface/L1TPFParticle.h"


namespace l1tpf {
    class HcalProducerFromOfflineRechits : public edm::stream::EDProducer<> {
        public:
            explicit HcalProducerFromOfflineRechits(const edm::ParameterSet&) ;
            ~HcalProducerFromOfflineRechits() {}

        private:
            edm::EDGetTokenT<HBHERecHitCollection> src_;
            double etCut_, eCut_;
            std::array<bool,15> subdets_;
            edm::ESHandle<CaloGeometry> pG;

            virtual void produce(edm::Event&, const edm::EventSetup&) override;
            
            struct SimpleHit { 
                float et, eta, phi; 
                SimpleHit(float aet, float aeta, float aphi) : et(aet), eta(aeta), phi(aphi) {}
            };

    }; // class
} // namespace

l1tpf::HcalProducerFromOfflineRechits::HcalProducerFromOfflineRechits(const edm::ParameterSet & iConfig) :
    src_(consumes<HBHERecHitCollection>(iConfig.getParameter<edm::InputTag>("src"))),
    etCut_(iConfig.getParameter<double>("etMin")),
    eCut_(iConfig.getParameter<double>("eMin")) 
{
    //produces<std::vector<l1tpf::Particle>>("crystals");
    produces<std::vector<l1tpf::Particle>>("towers");
    std::fill(subdets_.begin(), subdets_.end(), false);
    for (const auto sub : iConfig.getParameter<std::vector<uint32_t>>("subdets")) {
        subdets_[sub] = true;
    }
}


void 
l1tpf::HcalProducerFromOfflineRechits::produce(edm::Event &iEvent, const edm::EventSetup &iSetup) 
{
    //std::unique_ptr<std::vector<l1tpf::Particle>> out_crystal(new std::vector<l1tpf::Particle>());
    std::unique_ptr<std::vector<l1tpf::Particle>> out_tower(new std::vector<l1tpf::Particle>());

    std::map<std::pair<int,int>,std::vector<SimpleHit>> towers;

    //Get Calo Geometry
    iSetup.get<CaloGeometryRecord>().get(pG);
    const CaloGeometry* caloGeom = pG.product();

    edm::Handle<HBHERecHitCollection> src;
    iEvent.getByToken(src_, src);
    for (const HBHERecHit & hit : *src) {
        if (!subdets_[hit.id().subdet()]) continue;
        if (hit.energy() < eCut_) continue;
        const GlobalPoint & pos = caloGeom->getPosition(hit.detid());
        double et = pos.perp()/pos.mag() * hit.energy();
        if (et < etCut_) continue; 
        //out_crystal->emplace_back(et, pos.eta(), pos.phi(), 0, 0, 0, 0, pos.eta(), pos.phi());
        //out_crystal->back().setIEtaIPhi(hit.id().ieta(), hit.id().iphi());
        //towers[make_pair(hit.id().ieta(), hit.id().iphi())].emplace_back(et, pos.eta(), pos.phi());
	towers[make_pair(l1tpf::translateIEta(pos.eta()),l1tpf::translateIPhi(pos.phi(),pos.eta()))].emplace_back(et, pos.eta(), pos.phi());
    }
    for (const auto & pair : towers) {
        double etsum = 0., etaetsum = 0., phietsum = 0.; 
        double reta = pair.second.front().eta, rphi = pair.second.front().phi;
        for (const SimpleHit & hit : pair.second) {
            etsum += hit.et;
            etaetsum += (hit.eta - reta) * hit.et;
            phietsum += reco::deltaPhi(hit.phi, rphi) * hit.et;
        }
        etaetsum /= etsum;
        phietsum /= etsum;
        out_tower->emplace_back(etsum, etaetsum + reta, reco::deltaPhi(phietsum + rphi, 0.), 0, 0,0,0,etsum, etaetsum + reta, reco::deltaPhi(phietsum + rphi, 0.));
    }

    //iEvent.put(std::move(out_crystal), "crystals");
    iEvent.put(std::move(out_tower), "towers");
}
using l1tpf::HcalProducerFromOfflineRechits;
DEFINE_FWK_MODULE(HcalProducerFromOfflineRechits);
