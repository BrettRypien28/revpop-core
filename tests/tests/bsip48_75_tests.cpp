/*
 * Copyright (c) 2020 Abit More, and contributors.
 *
 * The MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "../common/database_fixture.hpp"

#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/asset_object.hpp>
#include <graphene/chain/market_object.hpp>
#include <graphene/chain/proposal_object.hpp>

#include <boost/test/unit_test.hpp>

using namespace graphene::chain;
using namespace graphene::chain::test;

BOOST_FIXTURE_TEST_SUITE( bsip48_75_tests, database_fixture )

BOOST_AUTO_TEST_CASE( hardfork_protection_test )
{
   try {

      // Proceeds to a recent hard fork
      generate_blocks( HARDFORK_CORE_1270_TIME );
      generate_block();
      set_expiration( db, trx );

      ACTORS((sam)(feeder));

      auto init_amount = 10000000 * GRAPHENE_BLOCKCHAIN_PRECISION;
      fund( sam, asset(init_amount) );
      fund( feeder, asset(init_amount) );

      uint16_t bitmask = ASSET_ISSUER_PERMISSION_ENABLE_BITS_MASK;
      uint16_t uiamask = DEFAULT_UIA_ASSET_ISSUER_PERMISSION;

      uint16_t bitflag = ~global_settle & ~committee_fed_asset; // high bits are set
      uint16_t uiaflag = ~(bitmask ^ uiamask); // high bits are set

      vector<operation> ops;

      // Testing asset_create_operation
      asset_create_operation acop;
      acop.issuer = sam_id;
      acop.symbol = "SAMCOIN";
      acop.precision = 2;
      acop.common_options.core_exchange_rate = price(asset(1,asset_id_type(1)),asset(1));
      acop.common_options.max_supply = GRAPHENE_MAX_SHARE_SUPPLY;
      acop.common_options.market_fee_percent = 100;
      acop.common_options.flags = uiaflag;
      acop.common_options.issuer_permissions = uiamask;

      trx.operations.clear();
      trx.operations.push_back( acop );

      {
         auto& op = trx.operations.front().get<asset_create_operation>();

         // Unable to set new permission bits
         op.common_options.issuer_permissions = ( uiamask | lock_max_supply );
         BOOST_CHECK_THROW( PUSH_TX(db, trx, ~0), fc::exception );
         ops.push_back( op );

         op.common_options.issuer_permissions = ( uiamask | disable_new_supply );
         BOOST_CHECK_THROW( PUSH_TX(db, trx, ~0), fc::exception );
         ops.push_back( op );

         op.bitasset_opts = bitasset_options();
         op.bitasset_opts->minimum_feeds = 3;
         op.common_options.flags = bitflag;

         op.common_options.issuer_permissions = ( bitmask | disable_mcr_update );
         BOOST_CHECK_THROW( PUSH_TX(db, trx, ~0), fc::exception );
         ops.push_back( op );

         op.common_options.issuer_permissions = ( bitmask | disable_icr_update );
         BOOST_CHECK_THROW( PUSH_TX(db, trx, ~0), fc::exception );
         ops.push_back( op );

         op.common_options.issuer_permissions = ( bitmask | disable_mssr_update );
         BOOST_CHECK_THROW( PUSH_TX(db, trx, ~0), fc::exception );
         ops.push_back( op );

         op.common_options.issuer_permissions = bitmask;

         // Unable to set new extensions in bitasset options
         op.bitasset_opts->extensions.value.maintenance_collateral_ratio = 1500;
         BOOST_CHECK_THROW( PUSH_TX(db, trx, ~0), fc::exception );
         ops.push_back( op );
         op.bitasset_opts->extensions.value.maintenance_collateral_ratio = {};

         op.bitasset_opts->extensions.value.maximum_short_squeeze_ratio = 1500;
         BOOST_CHECK_THROW( PUSH_TX(db, trx, ~0), fc::exception );
         ops.push_back( op );
         op.bitasset_opts->extensions.value.maximum_short_squeeze_ratio = {};

         acop = op;
      }

      // Able to create asset without new data
      processed_transaction ptx = PUSH_TX(db, trx, ~0);
      const asset_object& samcoin = db.get<asset_object>(ptx.operation_results[0].get<object_id_type>());
      asset_id_type samcoin_id = samcoin.id;

      BOOST_CHECK_EQUAL( samcoin.options.market_fee_percent, 100 );
      BOOST_CHECK_EQUAL( samcoin.bitasset_data(db).options.minimum_feeds, 3 );

      // Unable to propose the invalid operations
      for( const operation& op : ops )
         BOOST_CHECK_THROW( propose( op ), fc::exception );
      ops.clear();
      // Able to propose the good operation
      propose( acop );

      // Testing asset_update_operation
      asset_update_operation auop;
      auop.issuer = sam_id;
      auop.asset_to_update = samcoin_id;
      auop.new_options = samcoin_id(db).options;

      trx.operations.clear();
      trx.operations.push_back( auop );

      {
         auto& op = trx.operations.front().get<asset_update_operation>();
         op.new_options.market_fee_percent = 200;
         op.new_options.flags &= ~witness_fed_asset;

         // Unable to set new permission bits
         op.new_options.issuer_permissions = ( bitmask | lock_max_supply );
         BOOST_CHECK_THROW( PUSH_TX(db, trx, ~0), fc::exception );
         ops.push_back( op );

         op.new_options.issuer_permissions = ( bitmask | disable_new_supply );
         BOOST_CHECK_THROW( PUSH_TX(db, trx, ~0), fc::exception );
         ops.push_back( op );

         op.new_options.issuer_permissions = ( bitmask | disable_mcr_update );
         BOOST_CHECK_THROW( PUSH_TX(db, trx, ~0), fc::exception );
         ops.push_back( op );

         op.new_options.issuer_permissions = ( bitmask | disable_icr_update );
         BOOST_CHECK_THROW( PUSH_TX(db, trx, ~0), fc::exception );
         ops.push_back( op );

         op.new_options.issuer_permissions = ( bitmask | disable_mssr_update );
         BOOST_CHECK_THROW( PUSH_TX(db, trx, ~0), fc::exception );
         ops.push_back( op );

         op.new_options.issuer_permissions = bitmask;

         // Unable to set new extensions
         op.extensions.value.new_precision = 8;
         BOOST_CHECK_THROW( PUSH_TX(db, trx, ~0), fc::exception );
         ops.push_back( op );
         op.extensions.value.new_precision = {};

         op.extensions.value.skip_core_exchange_rate = true;
         BOOST_CHECK_THROW( PUSH_TX(db, trx, ~0), fc::exception );
         ops.push_back( op );
         op.extensions.value.skip_core_exchange_rate = {};

         auop = op;
      }

      // Able to update asset without new data
      PUSH_TX(db, trx, ~0);

      BOOST_CHECK_EQUAL( samcoin.options.market_fee_percent, 200 );

      // Unable to propose the invalid operations
      for( const operation& op : ops )
         BOOST_CHECK_THROW( propose( op ), fc::exception );
      ops.clear();
      // Able to propose the good operation
      propose( auop );


      // Testing asset_update_bitasset_operation
      asset_update_bitasset_operation aubop;
      aubop.issuer = sam_id;
      aubop.asset_to_update = samcoin_id;
      aubop.new_options = samcoin_id(db).bitasset_data(db).options;

      trx.operations.clear();
      trx.operations.push_back( aubop );

      {
         auto& op = trx.operations.front().get<asset_update_bitasset_operation>();
         op.new_options.minimum_feeds = 1;

         // Unable to set new extensions
         op.new_options.extensions.value.maintenance_collateral_ratio = 1500;
         BOOST_CHECK_THROW( PUSH_TX(db, trx, ~0), fc::exception );
         ops.push_back( op );
         op.new_options.extensions.value.maintenance_collateral_ratio = {};

         op.new_options.extensions.value.maximum_short_squeeze_ratio = 1500;
         BOOST_CHECK_THROW( PUSH_TX(db, trx, ~0), fc::exception );
         ops.push_back( op );
         op.new_options.extensions.value.maximum_short_squeeze_ratio = {};

         aubop = op;
      }

      // Able to update bitasset without new data
      PUSH_TX(db, trx, ~0);

      BOOST_CHECK_EQUAL( samcoin.bitasset_data(db).options.minimum_feeds, 1 );

      // Unable to propose the invalid operations
      for( const operation& op : ops )
         BOOST_CHECK_THROW( propose( op ), fc::exception );
      ops.clear();
      // Able to propose the good operation
      propose( aubop );

      // Testing asset_publish_feed_operation
      update_feed_producers( samcoin, { feeder_id } );

      price_feed f;
      f.settlement_price = price( asset(1,samcoin_id), asset(1) );
      f.core_exchange_rate = price( asset(1,samcoin_id), asset(1) );
      f.maintenance_collateral_ratio = 1850;

      asset_publish_feed_operation apfop;
      apfop.publisher = feeder_id;
      apfop.asset_id = samcoin_id;
      apfop.feed = f;

      trx.operations.clear();
      trx.operations.push_back( apfop );

      {
         auto& op = trx.operations.front().get<asset_publish_feed_operation>();

         // Unable to set new extensions
         op.extensions.value.initial_collateral_ratio = 1500;
         BOOST_CHECK_THROW( PUSH_TX(db, trx, ~0), fc::exception );
         ops.push_back( op );
         op.extensions.value.initial_collateral_ratio = {};

         apfop = op;
      }

      // Able to publish feed without new data
      PUSH_TX(db, trx, ~0);

      BOOST_CHECK_EQUAL( samcoin.bitasset_data(db).current_feed.initial_collateral_ratio,
                         f.maintenance_collateral_ratio );

      // Unable to propose the invalid operations
      for( const operation& op : ops )
         BOOST_CHECK_THROW( propose( op ), fc::exception );
      ops.clear();
      // Able to propose the good operation
      propose( apfop );

      // Check what we have now
      idump( (samcoin) );
      idump( (samcoin.bitasset_data(db)) );

      generate_block();

   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( update_max_supply )
{
   try {

      // Proceeds to a recent hard fork
      generate_blocks( HARDFORK_CORE_1270_TIME );
      generate_block();
      set_expiration( db, trx );

      ACTORS((sam));

      // create a UIA
      const asset_object& uia = create_user_issued_asset( "UIATEST", sam, charge_market_fee );
      asset_id_type uia_id = uia.id;

      // issue some to Sam
      issue_uia( sam_id, uia.amount( GRAPHENE_MAX_SHARE_SUPPLY - 100 ) );

      BOOST_CHECK( uia_id(db).can_update_max_supply() );
      BOOST_CHECK_EQUAL( uia_id(db).options.max_supply.value, GRAPHENE_MAX_SHARE_SUPPLY );
      BOOST_CHECK_EQUAL( uia_id(db).dynamic_data(db).current_supply.value, GRAPHENE_MAX_SHARE_SUPPLY - 100 );

      // update max supply to a smaller number
      asset_update_operation auop;
      auop.issuer = sam_id;
      auop.asset_to_update = uia_id;
      auop.new_options = uia_id(db).options;
      auop.new_options.max_supply -= 101;

      trx.operations.clear();
      trx.operations.push_back( auop );

      PUSH_TX(db, trx, ~0);

      BOOST_CHECK( uia_id(db).can_update_max_supply() );
      // max_supply < current_supply
      BOOST_CHECK_EQUAL( uia_id(db).options.max_supply.value, GRAPHENE_MAX_SHARE_SUPPLY - 101 );
      BOOST_CHECK_EQUAL( uia_id(db).dynamic_data(db).current_supply.value, GRAPHENE_MAX_SHARE_SUPPLY - 100 );

      // advance to bsip48/75 hard fork
      generate_blocks( HARDFORK_BSIP_48_75_TIME );
      set_expiration( db, trx );

      BOOST_CHECK_EQUAL( uia_id(db).dynamic_data(db).current_supply.value, uia_id(db).options.max_supply.value + 1 );
      BOOST_CHECK( uia_id(db).can_update_max_supply() );

      // able to set max supply to be equal to current supply
      auop.new_options.max_supply += 1;
      trx.operations.clear();
      trx.operations.push_back( auop );
      PUSH_TX(db, trx, ~0);

      BOOST_CHECK( uia_id(db).can_update_max_supply() );
      // max_supply == current_supply
      BOOST_CHECK_EQUAL( uia_id(db).options.max_supply.value, GRAPHENE_MAX_SHARE_SUPPLY - 100 );
      BOOST_CHECK_EQUAL( uia_id(db).dynamic_data(db).current_supply.value, GRAPHENE_MAX_SHARE_SUPPLY - 100 );

      // no longer able to set max supply to a number smaller than current supply
      auop.new_options.max_supply -= 1;
      trx.operations.clear();
      trx.operations.push_back( auop );
      BOOST_CHECK_THROW( PUSH_TX(db, trx, ~0), fc::exception );
      auop.new_options.max_supply += 1;

      BOOST_CHECK( uia_id(db).can_update_max_supply() );
      // max_supply == current_supply
      BOOST_CHECK_EQUAL( uia_id(db).options.max_supply.value, GRAPHENE_MAX_SHARE_SUPPLY - 100 );
      BOOST_CHECK_EQUAL( uia_id(db).dynamic_data(db).current_supply.value, GRAPHENE_MAX_SHARE_SUPPLY - 100 );

      // increase max supply again
      auop.new_options.max_supply += 2;
      trx.operations.clear();
      trx.operations.push_back( auop );
      PUSH_TX(db, trx, ~0);

      BOOST_CHECK( uia_id(db).can_update_max_supply() );
      // max_supply > current_supply
      BOOST_CHECK_EQUAL( uia_id(db).options.max_supply.value, GRAPHENE_MAX_SHARE_SUPPLY - 98 );
      BOOST_CHECK_EQUAL( uia_id(db).dynamic_data(db).current_supply.value, GRAPHENE_MAX_SHARE_SUPPLY - 100 );

      // decrease max supply
      auop.new_options.max_supply -= 1;
      trx.operations.clear();
      trx.operations.push_back( auop );
      PUSH_TX(db, trx, ~0);

      BOOST_CHECK( uia_id(db).can_update_max_supply() );
      // max_supply > current_supply
      BOOST_CHECK_EQUAL( uia_id(db).options.max_supply.value, GRAPHENE_MAX_SHARE_SUPPLY - 99 );
      BOOST_CHECK_EQUAL( uia_id(db).dynamic_data(db).current_supply.value, GRAPHENE_MAX_SHARE_SUPPLY - 100 );

      // update flag to disable updating of max supply
      auop.new_options.flags |= lock_max_supply;
      trx.operations.clear();
      trx.operations.push_back( auop );
      PUSH_TX(db, trx, ~0);

      BOOST_CHECK( !uia_id(db).can_update_max_supply() );
      // max_supply > current_supply
      BOOST_CHECK_EQUAL( uia_id(db).options.max_supply.value, GRAPHENE_MAX_SHARE_SUPPLY - 99 );
      BOOST_CHECK_EQUAL( uia_id(db).dynamic_data(db).current_supply.value, GRAPHENE_MAX_SHARE_SUPPLY - 100 );

      // unable to update max supply
      auop.new_options.max_supply -= 1;
      trx.operations.clear();
      trx.operations.push_back( auop );
      BOOST_CHECK_THROW( PUSH_TX(db, trx, ~0), fc::exception );
      auop.new_options.max_supply += 1;

      BOOST_CHECK( !uia_id(db).can_update_max_supply() );
      // max_supply > current_supply
      BOOST_CHECK_EQUAL( uia_id(db).options.max_supply.value, GRAPHENE_MAX_SHARE_SUPPLY - 99 );
      BOOST_CHECK_EQUAL( uia_id(db).dynamic_data(db).current_supply.value, GRAPHENE_MAX_SHARE_SUPPLY - 100 );

      // update flag to enable updating of max supply
      auop.new_options.flags &= ~lock_max_supply;
      trx.operations.clear();
      trx.operations.push_back( auop );
      PUSH_TX(db, trx, ~0);

      BOOST_CHECK( uia_id(db).can_update_max_supply() );
      // max_supply > current_supply
      BOOST_CHECK_EQUAL( uia_id(db).options.max_supply.value, GRAPHENE_MAX_SHARE_SUPPLY - 99 );
      BOOST_CHECK_EQUAL( uia_id(db).dynamic_data(db).current_supply.value, GRAPHENE_MAX_SHARE_SUPPLY - 100 );

      // able to update max supply
      auop.new_options.max_supply += 1;
      trx.operations.clear();
      trx.operations.push_back( auop );
      PUSH_TX(db, trx, ~0);

      BOOST_CHECK( uia_id(db).can_update_max_supply() );
      // max_supply > current_supply
      BOOST_CHECK_EQUAL( uia_id(db).options.max_supply.value, GRAPHENE_MAX_SHARE_SUPPLY - 98 );
      BOOST_CHECK_EQUAL( uia_id(db).dynamic_data(db).current_supply.value, GRAPHENE_MAX_SHARE_SUPPLY - 100 );

      // update flag to disable updating of max supply
      auop.new_options.flags |= lock_max_supply;
      // update permission to disable updating of lock_max_supply flag
      auop.new_options.issuer_permissions |= lock_max_supply;
      trx.operations.clear();
      trx.operations.push_back( auop );
      PUSH_TX(db, trx, ~0);

      BOOST_CHECK( !uia_id(db).can_update_max_supply() );
      // max_supply > current_supply
      BOOST_CHECK_EQUAL( uia_id(db).options.max_supply.value, GRAPHENE_MAX_SHARE_SUPPLY - 98 );
      BOOST_CHECK_EQUAL( uia_id(db).dynamic_data(db).current_supply.value, GRAPHENE_MAX_SHARE_SUPPLY - 100 );

      // Able to propose the operation
      propose( auop );

      // unable to reinstall the permission
      auop.new_options.issuer_permissions &= ~lock_max_supply;
      trx.operations.clear();
      trx.operations.push_back( auop );
      BOOST_CHECK_THROW( PUSH_TX(db, trx, ~0), fc::exception );
      auop.new_options.issuer_permissions |= lock_max_supply;

      BOOST_CHECK( !uia_id(db).can_update_max_supply() );
      // max_supply > current_supply
      BOOST_CHECK_EQUAL( uia_id(db).options.max_supply.value, GRAPHENE_MAX_SHARE_SUPPLY - 98 );
      BOOST_CHECK_EQUAL( uia_id(db).dynamic_data(db).current_supply.value, GRAPHENE_MAX_SHARE_SUPPLY - 100 );

      // unable to update max supply
      auop.new_options.max_supply -= 1;
      trx.operations.clear();
      trx.operations.push_back( auop );
      BOOST_CHECK_THROW( PUSH_TX(db, trx, ~0), fc::exception );
      auop.new_options.max_supply += 1;

      BOOST_CHECK( !uia_id(db).can_update_max_supply() );
      // max_supply > current_supply
      BOOST_CHECK_EQUAL( uia_id(db).options.max_supply.value, GRAPHENE_MAX_SHARE_SUPPLY - 98 );
      BOOST_CHECK_EQUAL( uia_id(db).dynamic_data(db).current_supply.value, GRAPHENE_MAX_SHARE_SUPPLY - 100 );

      // unable to enable the lock_max_supply flag
      auop.new_options.flags &= ~lock_max_supply;
      trx.operations.clear();
      trx.operations.push_back( auop );
      BOOST_CHECK_THROW( PUSH_TX(db, trx, ~0), fc::exception );
      auop.new_options.flags |= lock_max_supply;

      BOOST_CHECK( !uia_id(db).can_update_max_supply() );
      // max_supply > current_supply
      BOOST_CHECK_EQUAL( uia_id(db).options.max_supply.value, GRAPHENE_MAX_SHARE_SUPPLY - 98 );
      BOOST_CHECK_EQUAL( uia_id(db).dynamic_data(db).current_supply.value, GRAPHENE_MAX_SHARE_SUPPLY - 100 );

      // able to update other parameters
      auto old_market_fee_percent = auop.new_options.market_fee_percent;
      BOOST_CHECK_EQUAL( uia_id(db).options.market_fee_percent, old_market_fee_percent );

      auop.new_options.market_fee_percent = 120u;
      trx.operations.clear();
      trx.operations.push_back( auop );
      PUSH_TX(db, trx, ~0);

      BOOST_CHECK_EQUAL( uia_id(db).options.market_fee_percent, 120u );

      // reserve all supply
      reserve_asset( sam_id, uia_id(db).amount( GRAPHENE_MAX_SHARE_SUPPLY - 100 ) );

      BOOST_CHECK( !uia_id(db).can_update_max_supply() );
      // max_supply > current_supply
      BOOST_CHECK_EQUAL( uia_id(db).options.max_supply.value, GRAPHENE_MAX_SHARE_SUPPLY - 98 );
      BOOST_CHECK_EQUAL( uia_id(db).dynamic_data(db).current_supply.value, 0 );

      // still unable to update max supply
      auop.new_options.max_supply -= 1;
      trx.operations.clear();
      trx.operations.push_back( auop );
      BOOST_CHECK_THROW( PUSH_TX(db, trx, ~0), fc::exception );
      auop.new_options.max_supply += 1;

      BOOST_CHECK( !uia_id(db).can_update_max_supply() );
      // max_supply > current_supply
      BOOST_CHECK_EQUAL( uia_id(db).options.max_supply.value, GRAPHENE_MAX_SHARE_SUPPLY - 98 );
      BOOST_CHECK_EQUAL( uia_id(db).dynamic_data(db).current_supply.value, 0 );

      // still unable to enable the lock_max_supply flag
      auop.new_options.flags &= ~lock_max_supply;
      trx.operations.clear();
      trx.operations.push_back( auop );
      BOOST_CHECK_THROW( PUSH_TX(db, trx, ~0), fc::exception );
      auop.new_options.flags |= lock_max_supply;

      BOOST_CHECK( !uia_id(db).can_update_max_supply() );
      // max_supply > current_supply
      BOOST_CHECK_EQUAL( uia_id(db).options.max_supply.value, GRAPHENE_MAX_SHARE_SUPPLY - 98 );
      BOOST_CHECK_EQUAL( uia_id(db).dynamic_data(db).current_supply.value, 0 );

      // able to reinstall the permission and do it
      auop.new_options.issuer_permissions &= ~lock_max_supply;
      trx.operations.clear();
      trx.operations.push_back( auop );
      PUSH_TX(db, trx, ~0);

      BOOST_CHECK( !uia_id(db).can_update_max_supply() );
      // max_supply > current_supply
      BOOST_CHECK_EQUAL( uia_id(db).options.max_supply.value, GRAPHENE_MAX_SHARE_SUPPLY - 98 );
      BOOST_CHECK_EQUAL( uia_id(db).dynamic_data(db).current_supply.value, 0 );

      // still unable to update max supply
      auop.new_options.max_supply -= 1;
      trx.operations.clear();
      trx.operations.push_back( auop );
      BOOST_CHECK_THROW( PUSH_TX(db, trx, ~0), fc::exception );
      auop.new_options.max_supply += 1;

      BOOST_CHECK( !uia_id(db).can_update_max_supply() );
      // max_supply > current_supply
      BOOST_CHECK_EQUAL( uia_id(db).options.max_supply.value, GRAPHENE_MAX_SHARE_SUPPLY - 98 );
      BOOST_CHECK_EQUAL( uia_id(db).dynamic_data(db).current_supply.value, 0 );

      // now able to enable the lock_max_supply flag
      auop.new_options.flags &= ~lock_max_supply;
      trx.operations.clear();
      trx.operations.push_back( auop );
      PUSH_TX(db, trx, ~0);

      BOOST_CHECK( uia_id(db).can_update_max_supply() );
      // max_supply > current_supply
      BOOST_CHECK_EQUAL( uia_id(db).options.max_supply.value, GRAPHENE_MAX_SHARE_SUPPLY - 98 );
      BOOST_CHECK_EQUAL( uia_id(db).dynamic_data(db).current_supply.value, 0 );

      // update max supply
      auop.new_options.max_supply -= 1;
      trx.operations.clear();
      trx.operations.push_back( auop );
      PUSH_TX(db, trx, ~0);

      BOOST_CHECK( uia_id(db).can_update_max_supply() );
      // max_supply > current_supply
      BOOST_CHECK_EQUAL( uia_id(db).options.max_supply.value, GRAPHENE_MAX_SHARE_SUPPLY - 99 );
      BOOST_CHECK_EQUAL( uia_id(db).dynamic_data(db).current_supply.value, 0 );

      // issue some
      issue_uia( sam_id, uia_id(db).amount( 100 ) );

      BOOST_CHECK( uia_id(db).can_update_max_supply() );
      // max_supply > current_supply
      BOOST_CHECK_EQUAL( uia_id(db).options.max_supply.value, GRAPHENE_MAX_SHARE_SUPPLY - 99 );
      BOOST_CHECK_EQUAL( uia_id(db).dynamic_data(db).current_supply.value, 100 );

      // update permission to disable updating of lock_max_supply flag
      auop.new_options.issuer_permissions |= lock_max_supply;
      trx.operations.clear();
      trx.operations.push_back( auop );
      PUSH_TX(db, trx, ~0);

      // still can update max supply
      BOOST_CHECK( uia_id(db).can_update_max_supply() );
      // max_supply > current_supply
      BOOST_CHECK_EQUAL( uia_id(db).options.max_supply.value, GRAPHENE_MAX_SHARE_SUPPLY - 99 );
      BOOST_CHECK_EQUAL( uia_id(db).dynamic_data(db).current_supply.value, 100 );

      // unable to reinstall the permission
      auop.new_options.issuer_permissions &= ~lock_max_supply;
      trx.operations.clear();
      trx.operations.push_back( auop );
      BOOST_CHECK_THROW( PUSH_TX(db, trx, ~0), fc::exception );
      auop.new_options.issuer_permissions |= lock_max_supply;

      BOOST_CHECK( uia_id(db).can_update_max_supply() );
      // max_supply > current_supply
      BOOST_CHECK_EQUAL( uia_id(db).options.max_supply.value, GRAPHENE_MAX_SHARE_SUPPLY - 99 );
      BOOST_CHECK_EQUAL( uia_id(db).dynamic_data(db).current_supply.value, 100 );

      // update max supply
      auop.new_options.max_supply -= 1;
      trx.operations.clear();
      trx.operations.push_back( auop );
      PUSH_TX(db, trx, ~0);

      BOOST_CHECK( uia_id(db).can_update_max_supply() );
      // max_supply > current_supply
      BOOST_CHECK_EQUAL( uia_id(db).options.max_supply.value, GRAPHENE_MAX_SHARE_SUPPLY - 100 );
      BOOST_CHECK_EQUAL( uia_id(db).dynamic_data(db).current_supply.value, 100 );

      generate_block();

   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( disable_new_supply_uia )
{
   try {

      // advance to bsip48/75 hard fork
      generate_blocks( HARDFORK_BSIP_48_75_TIME );
      set_expiration( db, trx );

      ACTORS((sam));

      // create a UIA
      const asset_object& uia = create_user_issued_asset( "UIATEST", sam, charge_market_fee );
      asset_id_type uia_id = uia.id;

      BOOST_CHECK( uia_id(db).can_create_new_supply() );
      BOOST_CHECK_EQUAL( uia_id(db).dynamic_data(db).current_supply.value, 0 );

      // issue some to Sam
      issue_uia( sam_id, uia_id(db).amount( 100 ) );

      BOOST_CHECK( uia_id(db).can_create_new_supply() );
      BOOST_CHECK_EQUAL( uia_id(db).dynamic_data(db).current_supply.value, 100 );

      // prepare to update
      asset_update_operation auop;
      auop.issuer = sam_id;
      auop.asset_to_update = uia_id;
      auop.new_options = uia_id(db).options;

      // update flag to disable creation of new supply
      auop.new_options.flags |= disable_new_supply;
      trx.operations.clear();
      trx.operations.push_back( auop );
      PUSH_TX(db, trx, ~0);

      BOOST_CHECK( !uia_id(db).can_create_new_supply() );
      BOOST_CHECK_EQUAL( uia_id(db).dynamic_data(db).current_supply.value, 100 );

      // unable to issue more coins
      BOOST_CHECK_THROW( issue_uia( sam_id, uia_id(db).amount( 100 ) ), fc::exception );

      BOOST_CHECK( !uia_id(db).can_create_new_supply() );
      BOOST_CHECK_EQUAL( uia_id(db).dynamic_data(db).current_supply.value, 100 );

      // update flag to enable creation of new supply
      auop.new_options.flags &= ~disable_new_supply;
      trx.operations.clear();
      trx.operations.push_back( auop );
      PUSH_TX(db, trx, ~0);

      BOOST_CHECK( uia_id(db).can_create_new_supply() );
      BOOST_CHECK_EQUAL( uia_id(db).dynamic_data(db).current_supply.value, 100 );

      // issue some to Sam
      issue_uia( sam_id, uia_id(db).amount( 100 ) );

      BOOST_CHECK( uia_id(db).can_create_new_supply() );
      BOOST_CHECK_EQUAL( uia_id(db).dynamic_data(db).current_supply.value, 200 );

      // update flag to disable creation of new supply
      auop.new_options.flags |= disable_new_supply;
      // update permission to disable updating of disable_new_supply flag
      auop.new_options.issuer_permissions |= disable_new_supply;
      trx.operations.clear();
      trx.operations.push_back( auop );
      PUSH_TX(db, trx, ~0);

      BOOST_CHECK( !uia_id(db).can_create_new_supply() );
      BOOST_CHECK_EQUAL( uia_id(db).dynamic_data(db).current_supply.value, 200 );

      // Able to propose the operation
      propose( auop );

      // unable to reinstall the permission
      auop.new_options.issuer_permissions &= ~disable_new_supply;
      trx.operations.clear();
      trx.operations.push_back( auop );
      BOOST_CHECK_THROW( PUSH_TX(db, trx, ~0), fc::exception );
      auop.new_options.issuer_permissions |= disable_new_supply;

      BOOST_CHECK( !uia_id(db).can_create_new_supply() );
      BOOST_CHECK_EQUAL( uia_id(db).dynamic_data(db).current_supply.value, 200 );

      // unable to issue more coins
      BOOST_CHECK_THROW( issue_uia( sam_id, uia_id(db).amount( 100 ) ), fc::exception );

      BOOST_CHECK( !uia_id(db).can_create_new_supply() );
      BOOST_CHECK_EQUAL( uia_id(db).dynamic_data(db).current_supply.value, 200 );

      // unable to enable the disable_new_supply flag
      auop.new_options.flags &= ~disable_new_supply;
      trx.operations.clear();
      trx.operations.push_back( auop );
      BOOST_CHECK_THROW( PUSH_TX(db, trx, ~0), fc::exception );
      auop.new_options.flags |= disable_new_supply;

      BOOST_CHECK( !uia_id(db).can_create_new_supply() );
      BOOST_CHECK_EQUAL( uia_id(db).dynamic_data(db).current_supply.value, 200 );

      generate_block();

   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( skip_core_exchange_rate )
{
   try {

      // advance to bsip48/75 hard fork
      generate_blocks( HARDFORK_BSIP_48_75_TIME );
      set_expiration( db, trx );

      ACTORS((sam));

      // create a UIA
      const asset_object& uia = create_user_issued_asset( "UIATEST", sam, charge_market_fee );
      asset_id_type uia_id = uia.id;

      BOOST_CHECK( uia_id(db).options.core_exchange_rate == price(asset(1, uia_id), asset(1)) );

      // prepare to update
      asset_update_operation auop;
      auop.issuer = sam_id;
      auop.asset_to_update = uia_id;
      auop.new_options = uia_id(db).options;

      // update CER
      auop.new_options.core_exchange_rate = price(asset(2, uia_id), asset(1));
      trx.operations.clear();
      trx.operations.push_back( auop );
      PUSH_TX(db, trx, ~0);

      // CER changed
      BOOST_CHECK( uia_id(db).options.core_exchange_rate == price(asset(2, uia_id), asset(1)) );

      // save value for later check
      auto old_market_fee_percent = auop.new_options.market_fee_percent;
      BOOST_CHECK_EQUAL( uia_id(db).options.market_fee_percent, old_market_fee_percent );

      // set skip_core_exchange_rate to false, should fail
      auop.new_options.core_exchange_rate = price(asset(3, uia_id), asset(1));
      auop.extensions.value.skip_core_exchange_rate = false;
      trx.operations.clear();
      trx.operations.push_back( auop );
      BOOST_CHECK_THROW( PUSH_TX(db, trx, ~0), fc::exception );

      // unable to propose either
      BOOST_CHECK_THROW( propose( auop ), fc::exception );

      // CER didn't change
      BOOST_CHECK( uia_id(db).options.core_exchange_rate == price(asset(2, uia_id), asset(1)) );

      // skip updating CER
      auop.extensions.value.skip_core_exchange_rate = true;
      auop.new_options.market_fee_percent = 120u;
      trx.operations.clear();
      trx.operations.push_back( auop );
      PUSH_TX(db, trx, ~0);

      // CER didn't change
      BOOST_CHECK( uia_id(db).options.core_exchange_rate == price(asset(2, uia_id), asset(1)) );
      // market_fee_percent changed
      BOOST_CHECK_EQUAL( uia_id(db).options.market_fee_percent, 120u );

      // Able to propose the operation
      propose( auop );

      generate_block();

   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( invalid_flags_in_asset )
{
   try {

      // Proceeds to a recent hard fork
      generate_blocks( HARDFORK_CORE_1270_TIME );
      generate_block();
      set_expiration( db, trx );

      ACTORS((sam)(feeder));

      auto init_amount = 10000000 * GRAPHENE_BLOCKCHAIN_PRECISION;
      fund( sam, asset(init_amount) );
      fund( feeder, asset(init_amount) );

      uint16_t bitmask = ASSET_ISSUER_PERMISSION_ENABLE_BITS_MASK;
      uint16_t uiamask = DEFAULT_UIA_ASSET_ISSUER_PERMISSION;

      uint16_t bitflag = ~global_settle & ~committee_fed_asset; // high bits are set
      uint16_t uiaflag = ~(bitmask ^ uiamask); // high bits are set

      // Able to create UIA with invalid flags
      asset_create_operation acop;
      acop.issuer = sam_id;
      acop.symbol = "SAMCOIN";
      acop.precision = 2;
      acop.common_options.core_exchange_rate = price(asset(1,asset_id_type(1)),asset(1));
      acop.common_options.max_supply = GRAPHENE_MAX_SHARE_SUPPLY;
      acop.common_options.market_fee_percent = 100;
      acop.common_options.flags = uiaflag;
      acop.common_options.issuer_permissions = uiamask;

      trx.operations.clear();
      trx.operations.push_back( acop );

      processed_transaction ptx = PUSH_TX(db, trx, ~0);
      const asset_object& samcoin = db.get<asset_object>(ptx.operation_results[0].get<object_id_type>());
      asset_id_type samcoin_id = samcoin.id;

      // There are invalid bits in flags
      BOOST_CHECK( samcoin_id(db).options.flags & ~UIA_VALID_FLAGS_MASK );

      // Able to create MPA with invalid flags
      asset_create_operation acop2 = acop;
      acop2.symbol = "SAMBIT";
      acop2.bitasset_opts = bitasset_options();
      acop2.common_options.flags = bitflag;
      acop2.common_options.issuer_permissions = bitmask;

      trx.operations.clear();
      trx.operations.push_back( acop2 );

      ptx = PUSH_TX(db, trx, ~0);
      const asset_object& sambit = db.get<asset_object>(ptx.operation_results[0].get<object_id_type>());
      asset_id_type sambit_id = sambit.id;

      // There are invalid bits in flags
      BOOST_CHECK( sambit_id(db).options.flags & ~VALID_FLAGS_MASK );

      // Unable to correct the invalid flags of the UIA
      asset_update_operation auop;
      auop.issuer = sam_id;
      auop.asset_to_update = samcoin_id;
      auop.new_options = samcoin_id(db).options;
      auop.new_options.flags = 0;

      trx.operations.clear();
      trx.operations.push_back( auop );

      BOOST_CHECK_THROW( PUSH_TX(db, trx, ~0), fc::exception );

      // Unable to correct the invalid flags of the MPA
      asset_update_operation auop2;
      auop2.issuer = sam_id;
      auop2.asset_to_update = sambit_id;
      auop2.new_options = sambit_id(db).options;
      auop2.new_options.flags = 0;

      trx.operations.clear();
      trx.operations.push_back( auop2 );

      BOOST_CHECK_THROW( PUSH_TX(db, trx, ~0), fc::exception );

      // advance to bsip48/75 hard fork
      generate_blocks( HARDFORK_BSIP_48_75_TIME );
      set_expiration( db, trx );

      // take a look at flags of UIA
      BOOST_CHECK( samcoin_id(db).options.flags != UIA_VALID_FLAGS_MASK );

      // Try to update UIA but leave some invalid flags, should fail
      auop.new_options = samcoin_id(db).options;
      for( uint16_t bit = 0x8000; bit > 0; bit >>= 1 )
      {
         auop.new_options.flags = UIA_VALID_FLAGS_MASK | bit;
         if( auop.new_options.flags == UIA_VALID_FLAGS_MASK )
            continue;
         trx.operations.clear();
         trx.operations.push_back( auop );
         BOOST_CHECK_THROW( PUSH_TX(db, trx, ~0), fc::exception );
         // Unable to propose either if the bit is not a valid bit for MPA
         if( !(bit & VALID_FLAGS_MASK) )
            BOOST_CHECK_THROW( propose( auop ), fc::exception );
      }

      // Unset the invalid bits in flags, should succeed
      auop.new_options.flags = UIA_VALID_FLAGS_MASK;
      trx.operations.clear();
      trx.operations.push_back( auop );

      PUSH_TX(db, trx, ~0);

      BOOST_CHECK_EQUAL( samcoin_id(db).options.flags, UIA_VALID_FLAGS_MASK );

      // Able to propose too
      propose( auop );

      // take a look at flags of MPA
      uint16_t valid_bitflag = VALID_FLAGS_MASK & ~committee_fed_asset;
      BOOST_CHECK( sambit_id(db).options.flags != valid_bitflag );

      // Try to update MPA but leave some invalid flags, should fail
      auop2.new_options = sambit_id(db).options;
      for( uint16_t bit = 0x8000; bit > 0; bit >>= 1 )
      {
         auop2.new_options.flags = valid_bitflag | bit;
         if( auop2.new_options.flags == valid_bitflag )
            continue;
         trx.operations.clear();
         trx.operations.push_back( auop2 );
         BOOST_CHECK_THROW( PUSH_TX(db, trx, ~0), fc::exception );
         // Unable to propose either
         BOOST_CHECK_THROW( propose( auop2 ), fc::exception );
      }

      // Unset the invalid bits in flags, should succeed
      auop2.new_options.flags = valid_bitflag;
      trx.operations.clear();
      trx.operations.push_back( auop2 );
      PUSH_TX(db, trx, ~0);

      BOOST_CHECK_EQUAL( sambit_id(db).options.flags, valid_bitflag );

      // Able to propose too
      propose( auop2 );

      // Unable to create a new UIA with an unknown bit in flags
      acop.symbol = "NEWSAMCOIN";
      // With all possible bits in permissions set to 1
      acop2.common_options.issuer_permissions = UIA_ASSET_ISSUER_PERMISSION_MASK;
      for( uint16_t bit = 0x8000; bit > 0; bit >>= 1 )
      {
         acop.common_options.flags = UIA_VALID_FLAGS_MASK | bit;
         if( acop.common_options.flags == UIA_VALID_FLAGS_MASK )
            continue;
         trx.operations.clear();
         trx.operations.push_back( acop );
         BOOST_CHECK_THROW( PUSH_TX(db, trx, ~0), fc::exception );
         // Unable to propose either
         BOOST_CHECK_THROW( propose( acop ), fc::exception );
      }

      // Able to create a new UIA with a valid flags field
      acop.common_options.flags = UIA_VALID_FLAGS_MASK;
      trx.operations.clear();
      trx.operations.push_back( acop );
      ptx = PUSH_TX(db, trx, ~0);
      const asset_object& newsamcoin = db.get<asset_object>(ptx.operation_results[0].get<object_id_type>());
      asset_id_type newsamcoin_id = newsamcoin.id;

      BOOST_CHECK_EQUAL( newsamcoin_id(db).options.flags, UIA_VALID_FLAGS_MASK );

      // Able to propose too
      propose( acop );

      // Unable to create a new MPA with an unknown bit in flags
      acop2.symbol = "NEWSAMBIT";
      // With all possible bits in permissions set to 1
      acop2.common_options.issuer_permissions = ASSET_ISSUER_PERMISSION_MASK;
      for( uint16_t bit = 0x8000; bit > 0; bit >>= 1 )
      {
         acop2.common_options.flags = valid_bitflag | bit;
         if( acop2.common_options.flags == valid_bitflag )
            continue;
         trx.operations.clear();
         trx.operations.push_back( acop2 );
         BOOST_CHECK_THROW( PUSH_TX(db, trx, ~0), fc::exception );
         // Unable to propose either
         BOOST_CHECK_THROW( propose( acop2 ), fc::exception );
      }

      // Able to create a new MPA with a valid flags field
      acop2.common_options.flags = valid_bitflag;
      trx.operations.clear();
      trx.operations.push_back( acop2 );
      ptx = PUSH_TX(db, trx, ~0);
      const asset_object& newsambit = db.get<asset_object>(ptx.operation_results[0].get<object_id_type>());
      asset_id_type newsambit_id = newsambit.id;

      BOOST_CHECK_EQUAL( newsambit_id(db).options.flags, valid_bitflag );

      BOOST_CHECK( !newsambit_id(db).can_owner_update_icr() );
      BOOST_CHECK( !newsambit_id(db).can_owner_update_mcr() );
      BOOST_CHECK( !newsambit_id(db).can_owner_update_mssr() );

      // Able to propose too
      propose( acop2 );

      generate_block();

   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( update_asset_precision )
{
   try {

      // advance to bsip48/75 hard fork
      generate_blocks( HARDFORK_BSIP_48_75_TIME );
      set_expiration( db, trx );

      ACTORS((sam));

      // create a prediction market
      const asset_object& pm = create_prediction_market( "PDM", sam_id );
      asset_id_type pm_id = pm.id;

      BOOST_CHECK_EQUAL( pm_id(db).precision, 5 );

      // prepare to update
      asset_update_operation auop;
      auop.issuer = sam_id;
      auop.asset_to_update = pm_id;
      auop.new_options = pm_id(db).options;

      // Unable to update precision of a PM
      auop.extensions.value.new_precision = 4;
      trx.operations.clear();
      trx.operations.push_back( auop );
      BOOST_CHECK_THROW( PUSH_TX(db, trx, ~0), fc::exception );

      BOOST_CHECK_EQUAL( pm_id(db).precision, 5 );

      // Able to propose the operation
      propose( auop );

      // create a UIA
      const asset_object& uia = create_user_issued_asset( "UIATEST", sam, charge_market_fee );
      asset_id_type uia_id = uia.id;

      BOOST_CHECK_EQUAL( uia_id(db).precision, 2 );

      // try to set new precision to be the same as the old precision, will fail
      auop.asset_to_update = uia_id;
      auop.new_options = uia_id(db).options;
      auop.extensions.value.new_precision = 2;
      trx.operations.clear();
      trx.operations.push_back( auop );
      BOOST_CHECK_THROW( PUSH_TX(db, trx, ~0), fc::exception );

      BOOST_CHECK_EQUAL( uia_id(db).precision, 2 );

      // try to set new precision to a number which is too big, will fail
      auop.asset_to_update = uia_id;
      auop.new_options = uia_id(db).options;
      auop.extensions.value.new_precision = 13;
      trx.operations.clear();
      trx.operations.push_back( auop );
      BOOST_CHECK_THROW( PUSH_TX(db, trx, ~0), fc::exception );

      // Unable to propose either
      BOOST_CHECK_THROW( propose( auop ), fc::exception );

      BOOST_CHECK_EQUAL( uia_id(db).precision, 2 );

      // update precision to a valid number, should succeed
      auop.extensions.value.new_precision = 3;
      trx.operations.clear();
      trx.operations.push_back( auop );
      PUSH_TX(db, trx, ~0);

      BOOST_CHECK_EQUAL( uia_id(db).precision, 3 );

      // create some supply
      issue_uia( sam_id, asset( 100, uia_id ) );

      BOOST_CHECK_EQUAL( uia_id(db).dynamic_data(db).current_supply.value, 100 );

      // try to update precision, will fail
      auop.extensions.value.new_precision = 4;
      trx.operations.clear();
      trx.operations.push_back( auop );
      BOOST_CHECK_THROW( PUSH_TX(db, trx, ~0), fc::exception );

      BOOST_CHECK_EQUAL( uia_id(db).precision, 3 );

      // destroy all supply
      reserve_asset( sam_id, asset( 100, uia_id ) );

      BOOST_CHECK_EQUAL( uia_id(db).dynamic_data(db).current_supply.value, 0 );

      // update precision, should succeed
      auop.extensions.value.new_precision = 4;
      trx.operations.clear();
      trx.operations.push_back( auop );
      PUSH_TX(db, trx, ~0);

      BOOST_CHECK_EQUAL( uia_id(db).precision, 4 );

      // create a MPA which is backed by the UIA
      const asset_object& mpa = create_bitasset( "TESTBIT", sam_id, 10, charge_market_fee, 3, uia_id );
      asset_id_type mpa_id = mpa.id;

      BOOST_CHECK( mpa_id(db).bitasset_data(db).options.short_backing_asset == uia_id );

      // try to update precision of the UIA, will fail
      auop.extensions.value.new_precision = 3;
      trx.operations.clear();
      trx.operations.push_back( auop );
      BOOST_CHECK_THROW( PUSH_TX(db, trx, ~0), fc::exception );

      BOOST_CHECK_EQUAL( uia_id(db).precision, 4 );

      generate_block();

   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}


BOOST_AUTO_TEST_SUITE_END()

