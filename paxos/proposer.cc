#include "proposer.hh"

#include "absl/log/check.h"

void Proposer::Propose(
    const std::vector<std::unique_ptr<paxos::Acceptor::Stub>>& stubs,
    const std::string& value )
{
  bool done = false;
  int retry_count = 0;
  uint64_t index = first_uncommited_index_;
  while ( !done && retry_count < retry_count_ ) {
    std::string value_for_accept_phase = value;
    uint32_t num_promises = 0;

    paxos::PrepareRequest request;
    request.set_index( index );
    do {
      num_promises = 0;
      request.set_proposal_number( GetNextProposalNumber() );

      uint64_t max_proposal_id = 0;
      for ( size_t i = 0; i < stubs.size(); i++ ) {
        if ( stubs[i] ) {
          paxos::PrepareResponse response;
          grpc::ClientContext context;
          grpc::Status status
              = stubs[i]->Prepare( &context, request, &response );
          if ( !status.ok() ) {
            LOG( WARNING ) << "Prepare grpc failed for node: " << i
                           << " with error code: " << status.error_code()
                           << " and error message: " << status.error_message();
            continue;
          }

          if ( response.min_proposal() > request.proposal_number() ) {
            proposal_number_ = response.min_proposal();
            num_promises = 0;
            LOG( INFO ) << "Saw a proposal number larger than what we sent, "
                           "retry Propose operation with a bigger proposal.";
            break;
          }
          if ( response.has_accepted_value() ) {
            if ( max_proposal_id < response.accepted_proposal() ) {
              max_proposal_id = response.accepted_proposal();
              value_for_accept_phase = response.accepted_value();
            }
          }
          ++num_promises;
        }
      }
    } while ( num_promises < majority_threshold_ );

    paxos::AcceptRequest accept_request;
    accept_request.set_proposal_number( request.proposal_number() );
    accept_request.set_index( request.index() );
    accept_request.set_value( value_for_accept_phase );
    paxos::AcceptResponse accept_response;
    uint32_t accept_majority_count = 0;
    for ( size_t i = 0; i < stubs.size(); i++ ) {
      if ( stubs[i] ) {
        grpc::ClientContext context;
        grpc::Status status
            = stubs[i]->Accept( &context, accept_request, &accept_response );
        if ( !status.ok() ) {
          LOG( WARNING ) << "Accept grpc failed for node: " << i
                         << " with error code: " << status.error_code()
                         << " and error message: " << status.error_message();
          continue;
        }
        if ( accept_response.min_proposal() > request.proposal_number() ) {
          proposal_number_ = accept_response.min_proposal();
          accept_majority_count = 0;
          break;
        }
        ++accept_majority_count;
        LOG( INFO ) << "Got accept for proposal number: "
                    << accept_response.min_proposal()
                    << " and accepted value: " << value_for_accept_phase;
      }
    }
    if ( accept_majority_count >= majority_threshold_ ) {
      accepted_proposals_[request.index()] = value_for_accept_phase;
      ++first_uncommited_index_;
      LOG( INFO ) << "Accepted Proposal number: "
                  << accept_response.min_proposal()
                  << ", accepted value: " << value_for_accept_phase
                  << ", at index: " << request.index() << "\n";
      index = first_uncommited_index_;
      // Maybe instead check if response.has_accepted_value()? That would avoid
      // a string compare.
      if ( value == value_for_accept_phase ) { done = true; }
    } else if ( retry_count > retry_count_ ) {
      LOG( ERROR ) << "Failed to reach consensus\n";
    }
  }
}