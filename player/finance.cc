/*
 * This file is part of the Simutrans-Extended project under the Artistic License.
 * (see LICENSE.txt)
 */

#include <stdio.h>
#include <assert.h>

#include "../utils/float32e8_t.h"
#include "../simworld.h"
#include "../descriptor/building_desc.h"
#include "../dataobj/loadsave.h"
#include "../dataobj/scenario.h"
#include "simplay.h"

#include "finance.h"


/**
 * initialize finance history arrays
 */
finance_t::finance_t(player_t * _player, karte_t * _world) :
	player(_player),
	world(_world)
{
	account_balance = world->get_settings().get_starting_money(world->get_last_year());
	starting_money = account_balance;
	account_overdrawn = 0;

	for (int year=0; year<MAX_PLAYER_HISTORY_YEARS; year++) {
		for (int cost_type=0; cost_type<ATC_MAX; cost_type++) {
			com_year[year][cost_type] = 0;
			if ((cost_type == ATC_CASH) || (cost_type == ATC_NETWEALTH)) {
				com_year[year][cost_type] = starting_money;
			}
		}
	}

	for (int month=0; month<MAX_PLAYER_HISTORY_MONTHS; month++) {
		for (int cost_type=0; cost_type<ATC_MAX; cost_type++) {
			com_month[month][cost_type] = 0;
			if ((cost_type == ATC_CASH) || (cost_type == ATC_NETWEALTH)) {
				com_month[month][cost_type] = starting_money;
			}
		}
	}

	for (int transport_type=0; transport_type<TT_MAX; ++transport_type){
		for (int year=0; year<MAX_PLAYER_HISTORY_YEARS; year++) {
			for (int cost_type=0; cost_type<ATV_MAX; cost_type++) {
				veh_year[transport_type][year][cost_type] = 0;
			}
		}
	}

	for (int transport_type=0; transport_type<TT_MAX; ++transport_type){
		for (int month=0; month<MAX_PLAYER_HISTORY_MONTHS; month++) {
			for (int cost_type=0; cost_type<ATV_MAX; cost_type++) {
				veh_month[transport_type][month][cost_type] = 0;
			}
		}
	}

	for(int i=0; i<TT_MAX; ++i){
		maintenance[i] = 0;
	}

	// for(int i=0; i<TT_MAX_VEH; ++i){
	//	vehicle_maintenance[i] = 0;
	// }

	// calc_credit_limits()
	// We should recalculate credit limits here.
	// However, the starting credit limit will be zero
	// (no assets, no profits)
}


void finance_t::calc_finance_history()
{
	// vehicles
	for(int tt=1; tt<TT_MAX; ++tt){
		// ATV_REVENUE_TRANSPORT = ATV_REVENUE_PAS+MAIL+GOOD
		sint64 revenue, mrevenue;
		revenue = mrevenue = 0;
		for(int i=0; i<ATV_REVENUE_TRANSPORT; ++i){
			mrevenue += veh_month[tt][0][i];
			revenue  += veh_year[ tt][0][i];
		}
		veh_month[tt][0][ATV_REVENUE_TRANSPORT] = mrevenue;
		veh_year[ tt][0][ATV_REVENUE_TRANSPORT] = revenue;

		// ATV_REVENUE = ATV_REVENUE_TRANSPORT + ATV_TOLL_RECEIVED
		veh_month[tt][0][ATV_REVENUE] = veh_month[tt][0][ATV_REVENUE_TRANSPORT] + veh_month[tt][0][ATV_TOLL_RECEIVED];
		veh_year[tt][0][ATV_REVENUE] = veh_year[tt][0][ATV_REVENUE_TRANSPORT] + veh_year[tt][0][ATV_TOLL_RECEIVED];

		// ATC_EXPENDITURE = ATC_RUNNING_COST + ATC_VEH_MAINTENENCE + ATC_INF_MAINTENANCE + ATC_TOLL_PAID;
		sint64 expenditure, mexpenditure;
		expenditure = mexpenditure = 0;
		for(int i=ATV_RUNNING_COST; i<ATV_EXPENDITURE; ++i){
			mexpenditure += veh_month[tt][0][i];
			expenditure  += veh_year[ tt][0][i];
		}
		veh_month[tt][0][ATV_EXPENDITURE] = mexpenditure;
		veh_year[ tt][0][ATV_EXPENDITURE] = expenditure;
		veh_month[tt][0][ATV_OPERATING_PROFIT] = veh_month[tt][0][ATV_REVENUE] + mexpenditure;
		veh_year[ tt][0][ATV_OPERATING_PROFIT] = veh_year[ tt][0][ATV_REVENUE] +  expenditure;

		// PROFIT = OPERATING_PROFIT + NEW_VEHICLES + construction costs + interest
		sint64 profit, mprofit;
		profit = mprofit = 0;
		for(int i=ATV_OPERATING_PROFIT; i<ATV_PROFIT; ++i){
			mprofit += veh_month[tt][0][i];
			profit  += veh_year[ tt][0][i];
		}
		veh_month[tt][0][ATV_PROFIT] = mprofit;
		veh_year[ tt][0][ATV_PROFIT] =  profit;

		veh_month[tt][0][ATV_WAY_TOLL] = veh_month[tt][0][ATV_TOLL_RECEIVED] + veh_month[tt][0][ATV_TOLL_PAID];
		veh_year[ tt][0][ATV_WAY_TOLL] = veh_year[tt][0][ATV_TOLL_RECEIVED] + veh_year[tt][0][ATV_TOLL_PAID];

		veh_month[tt][0][ATV_PROFIT_MARGIN] = calc_margin(veh_month[tt][0][ATV_OPERATING_PROFIT], veh_month[tt][0][ATV_REVENUE]);
		veh_year[tt][0][ATV_PROFIT_MARGIN] = calc_margin(veh_year[tt][0][ATV_OPERATING_PROFIT], veh_year[tt][0][ATV_REVENUE]);

		sint64 transported = 0, mtransported = 0;
		for(int i=ATV_TRANSPORTED_PASSENGER; i<ATV_TRANSPORTED; ++i){
			mtransported += veh_month[tt][0][i];
			transported  += veh_year[ tt][0][i];
		}
		veh_month[tt][0][ATV_TRANSPORTED] = mtransported;
		veh_year[ tt][0][ATV_TRANSPORTED] =  transported;

		sint64 delivered = 0, mdelivered = 0;
		for(int i=ATV_DELIVERED_PASSENGER; i<ATV_DELIVERED; ++i){
			mdelivered += veh_month[tt][0][i];
			delivered  += veh_year[ tt][0][i];
		}
		veh_month[tt][0][ATV_DELIVERED] = mdelivered;
		veh_year[ tt][0][ATV_DELIVERED] =  delivered;
	}

	// sum up statistic for all transport types
	for( int j=0; j< ATV_MAX; ++j ) {
		veh_month[TT_ALL][0][j] =0;
		for( int tt=1; tt<TT_MAX; ++tt ) {
			// do not add powerline revenue to vehicles revenue
			if ( ( tt != TT_POWERLINE ) || ( j >= ATV_REVENUE )) {
				veh_month[TT_ALL][0][j] += veh_month[tt][0][j];
			}
		}
	}
	for( int j=0; j< ATV_MAX; ++j ) {
		veh_year[TT_ALL][0][j] =0;
		for( int tt=1; tt<TT_MAX; ++tt ) {
			// do not add powerline revenue to vehicles revenue
			if ( ( tt != TT_POWERLINE ) || ( j >= ATV_REVENUE )) {
				veh_year[TT_ALL][0][j] += veh_year[tt][0][j];
			}
		}
	}
	// recalc margin for TT_ALL
	veh_month[TT_ALL][0][ATV_PROFIT_MARGIN] = calc_margin(veh_month[TT_ALL][0][ATV_OPERATING_PROFIT], veh_month[TT_ALL][0][ATV_REVENUE]);
	veh_year[TT_ALL][0][ATV_PROFIT_MARGIN] = calc_margin(veh_year[TT_ALL][0][ATV_OPERATING_PROFIT], veh_year[TT_ALL][0][ATV_REVENUE]);
	veh_month[TT_ALL][0][ATV_PROFIT] += com_month[0][ATC_INTEREST];
	veh_year[TT_ALL][0][ATV_PROFIT] += com_year[0][ATC_INTEREST];
	veh_month[TT_OTHER][0][ATV_PROFIT] += com_month[0][ATC_INTEREST];
	veh_year[TT_OTHER][0][ATV_PROFIT] += com_year[0][ATC_INTEREST];

	// undistinguishable by type of transport
	com_month[0][ATC_CASH] = account_balance;
	com_year [0][ATC_CASH] = account_balance;
	com_month[0][ATC_NETWEALTH] = veh_month[TT_ALL][0][ATV_NON_FINANCIAL_ASSETS] + account_balance;
	com_year [0][ATC_NETWEALTH] = veh_year[TT_ALL][0][ATV_NON_FINANCIAL_ASSETS] + account_balance;
}


sint64 finance_t::get_maintenance_with_bits(transport_type tt) const
{
	assert(tt<TT_MAX);
	return world->calc_adjusted_monthly_figure( maintenance[tt] );
}

void finance_t::new_month()
{
	// First, make sure everything is recorded right, before changing the month.
	calc_finance_history();
	roll_history_month();

	if(world->get_last_month()==0) {
		roll_history_year();
	}

	// subtract interest (before subtracting infrastructure maintenance)
	book_interest_monthly();

	// subtract infrastructure maintenance
	for(int i=0; i<TT_MAX; ++i){
		veh_month[i][0][ATV_INFRASTRUCTURE_MAINTENANCE] -= get_maintenance_with_bits((transport_type)i);
		veh_year [i][0][ATV_INFRASTRUCTURE_MAINTENANCE] -= get_maintenance_with_bits((transport_type)i);
	}

}

/**
 * Books interest expense or profit.
 */
void finance_t::book_interest_monthly()
{
	// This handles both interest on cash balance and interest on loans.
	// Rate is yearly rate for debt; rate for credit is 1/4 of that.  (Fix this.)
	const sint64 interest_rate = (sint64)world->get_settings().get_interest_rate_percent();
	if (interest_rate > 0)
	{
		/*float32e8_t interest (interest_rate);
		interest /= (float32e8_t)100; // percent
		interest /= (float32e8_t)12; // monthly
		if (get_account_balance() >= 0) {
			// Credit interest rate is 1/4 of debt interest rate.
			interest /= (float32e8_t)4;
		}
		// Apply to the current account balance, only if in debt.
		// Credit interest, which applied in earlier versions, unbalanced the game.
		interest *= (float32e8_t)get_account_balance();
		// Due to the limitations of float32e8, interest can only go up to +-2^31 per month.
		// Hopefully this won't be an issue.  It will report errors if it is.
		// This would require an account balance of over +-257 billion.
		sint32 booked_interest = interest;*/

		sint64 interest;
		if(get_account_balance() < 0)
		{
			interest = (interest_rate * get_account_balance()) / 1200ll;
		}
		else
		{
			interest = 0;
		}

		com_year[0][ATC_INTEREST] += interest;
		com_month[0][ATC_INTEREST] += interest;
		account_balance += interest;
	}
}

void finance_t::calc_credit_limits()
{
	sint64 hard_limit_by_profits = credit_limit_by_profits();
	sint64 hard_limit_by_assets = credit_limit_by_assets();

	// The player gets the better of the two credit limits.  Remember that they are negative.
	sint64 hard_credit_limit = std::min(hard_limit_by_profits, hard_limit_by_assets);
	assert(hard_credit_limit <= 0);

	// Soft credit limit is a percentage of the hard credit limit
	// This percentage will be set by settings -- FIXME LATER
	uint8 soft_credit_limit_percent = 50;

	// Don't worry about exact computations here.
	sint64 soft_credit_limit = (hard_credit_limit / 100) * soft_credit_limit_percent;
	assert(soft_credit_limit <= 0);

	com_month[0][ATC_HARD_CREDIT_LIMIT] = hard_credit_limit;
	com_month[0][ATC_SOFT_CREDIT_LIMIT] = soft_credit_limit;

	if(world->get_last_month()==0) {
		// New year, record new starting credit limit
		com_year[0][ATC_HARD_CREDIT_LIMIT] = hard_credit_limit;
		com_year[0][ATC_SOFT_CREDIT_LIMIT] = soft_credit_limit;
	}
}

/**
 * Subroutine for credit limit calculuations.
 * Calculates a credit limit based on past year's profitability
 * (ability to cover interest costs).
 */
sint64 finance_t::credit_limit_by_profits() const
{
	// The idea is that yearly profits should cover yearly interest
	// Look back 12 months (full year's profit)
	sint64 profit_total=0;
	// We need 12 months of history at least, not including this month
	assert(MAX_PLAYER_HISTORY_MONTHS >= 13);
	// Start by looking at *last* month and go back 12 (one year)
	for(int month = 1; month < 13; month++)
	{
		// Use operating profits as the basis for interest coverage.
		// This is before interst and before construction costs.
		profit_total += get_history_veh_month(TT_ALL, month, ATV_OPERATING_PROFIT);
    }
	sint64 interest_rate = world->get_settings().get_interest_rate_percent();

	if(interest_rate == 0)
	{
		return 0;
	}

	// *Divide* by the interest rate: if all the profits went to interest,
	// this tells us how much debt (principal) we could pay interest on
	// Does not account for compound interest, so generous to the player
	sint64 hard_limit_by_profits = - (profit_total * 100ll) / interest_rate;
	// The following deals with recurring losses;
	// It also deals (badly) with overflow errors.
	if (hard_limit_by_profits > 0) {
		hard_limit_by_profits = 0;
	}
	return hard_limit_by_profits;
}

/**
 * Subroutine for credit limit calculations.
 * Calculates an asset-based credit limit.
 * Secured borrowing against assets.
 */
sint64 finance_t::credit_limit_by_assets() const
{
	// Can borrow against potentially all assets.
	sint64 hard_limit_by_assets = - get_history_veh_month(TT_ALL, 0, ATV_NON_FINANCIAL_ASSETS);
	// The following deals with potential bugs.
    if (hard_limit_by_assets > 0) {
        hard_limit_by_assets = 0;
    }
	return hard_limit_by_assets;
}

/* most recent savegame version: now with detailed finance statistics by type of transport */
void finance_t::rdwr(loadsave_t *file)
{
	// detailed statistic were introduced in this version
	if( file->is_version_less(112, 5) ) {
		rdwr_compatibility(file);
		if ( file->is_loading() ) {
			// Loaded hard credit limit will be wrong, fix it quick to avoid spurious insolvency
			calc_credit_limits();
		}
		return;
	}

	/* following lines enables FORWARD compatibility
	/ you will be still able to load future versions of games with:
	*   longer history
	*   more transport_types
	*   and new items in ATC_ or ATV_
	* Warning: extended adds three lines to ATC_ immediately, with version 112005.
	* If Standard adds lines to ATC_, we must make adjustments by pushing the extended lines "down".
	*/
	sint8 max_years  = MAX_PLAYER_HISTORY_YEARS;
	sint8 max_months = MAX_PLAYER_HISTORY_MONTHS;
	sint8 max_tt     = TT_MAX;
	sint8 max_atc    = ATC_MAX;
	sint8 max_atv    = ATV_MAX;

	// used for reading longer history
	sint64 dummy = 0;

	// calc finance history for TT_ALL to save it correctly
	if( ! file->is_loading() ) {
		calc_finance_history();
	}

	file->rdwr_longlong(account_balance);
	file->rdwr_long(account_overdrawn);
	file->rdwr_longlong(starting_money);

	file->rdwr_byte( max_years );
	file->rdwr_byte( max_months );
	file->rdwr_byte( max_tt ); // tt = transport type
	file->rdwr_byte( max_atc ); // atc = accounting type common
	file->rdwr_byte( max_atv ); // atv = accounting type vehicles

	for(int year = 0;  year < max_years ; ++year ) {
		for( int cost_type = 0; cost_type < max_atc ;  ++cost_type  ) {
			if( ( year < MAX_PLAYER_HISTORY_YEARS ) && ( cost_type < ATC_MAX ) ) {
				file->rdwr_longlong( com_year[year][cost_type] );
			} else {
				file->rdwr_longlong( dummy );
			}
		}
	}
	for(int month = 0; month < max_months; ++month) {
		for( int cost_type = 0; cost_type < max_atc;  ++cost_type ) {
			if( ( month < MAX_PLAYER_HISTORY_MONTHS ) && ( cost_type < ATC_MAX ) ) {
				file->rdwr_longlong( com_month[month][cost_type] );
			} else {
				file->rdwr_longlong( dummy );
			}
		}
	}
	for(int tt=0; tt < max_tt; ++tt){
		for( int year = 0;  year < max_years;  ++year ) {
			for( int cost_type = 0; cost_type < max_atv;  ++cost_type  ) {
				if( ( tt < TT_MAX ) && ( year < MAX_PLAYER_HISTORY_YEARS ) && ( cost_type < ATV_MAX ) ) {
					file->rdwr_longlong( veh_year[tt][year][cost_type] );
				} else {
					file->rdwr_longlong( dummy );
				}
			}
		}
	}
	for(int tt=0; tt < max_tt; ++tt){
		for( int month = 0; month < max_months; ++month ) {
			for( int cost_type = 0; cost_type < max_atv;  ++cost_type  ) {
				if( ( tt < TT_MAX ) && ( month < MAX_PLAYER_HISTORY_MONTHS ) && ( cost_type < ATV_MAX ) ) {
					file->rdwr_longlong( veh_month[tt][month][cost_type] );
				} else {
					file->rdwr_longlong( dummy );
				}
			}
		}
	}
}


void finance_t::roll_history_month()
{
	// undistinguishable
	for (int i=MAX_PLAYER_HISTORY_MONTHS-1; i>0; i--) {
		for(int accounting_type=0; accounting_type<ATC_MAX; ++accounting_type){
			com_month[i][accounting_type] = com_month[i-1][accounting_type];
		}
	}
	for(int i=0; i<ATC_MAX; ++i){
		if(i != ATC_ALL_CONVOIS  &&  i != ATC_SCENARIO_COMPLETED){
			com_month[0][i] = 0;
		}
	}
	// vehicles
	for(int tt=0; tt<TT_MAX; ++tt){
		for (int i=MAX_PLAYER_HISTORY_MONTHS-1; i>0; i--) {
			for(int accounting_type=0; accounting_type<ATV_MAX; ++accounting_type){
				veh_month[tt][i][accounting_type] = veh_month[tt][i-1][accounting_type];
			}
		}
		for(int accounting_type=0; accounting_type<ATV_MAX; ++accounting_type){
			veh_month[tt][0][accounting_type] = 0;
		}
	}
}


void finance_t::roll_history_year()
{
	// undistinguishable
	for (int i=MAX_PLAYER_HISTORY_YEARS-1; i>0; i--) {
		for(int accounting_type=0; accounting_type<ATC_MAX; ++accounting_type){
			com_year[i][accounting_type] = com_year[i-1][accounting_type];
		}
	}
	for(int i=0; i<ATC_MAX; ++i){
		if(i != ATC_ALL_CONVOIS  &&  i != ATC_SCENARIO_COMPLETED){
			com_year[0][i] = 0;
		}
	}
	// vehicles
	for(int tt=0; tt<TT_MAX; ++tt){
		for (int i=MAX_PLAYER_HISTORY_YEARS-1; i>0; i--) {
			for(int accounting_type=0; accounting_type<ATV_MAX; ++accounting_type){
				veh_year[tt][i][accounting_type] = veh_year[tt][i-1][accounting_type];
			}
		}
		for(int accounting_type=0; accounting_type<ATV_MAX; ++accounting_type){
			veh_year[tt][0][accounting_type] = 0;
		}
	}
}


void finance_t::set_assets(const sint64 (&assets)[TT_MAX])
{
	for(int i=0; i < TT_MAX; ++i){
		veh_year[i][0][ATV_NON_FINANCIAL_ASSETS] = veh_month[i][0][ATV_NON_FINANCIAL_ASSETS] = assets[i];
	}
	com_year[0][ATC_NETWEALTH] = com_month[0][ATC_NETWEALTH] = veh_month[TT_ALL][0][ATV_NON_FINANCIAL_ASSETS] + account_balance;
}


void finance_t::update_assets(sint64 const delta, const waytype_t wt)
{
	transport_type tt = translate_waytype_to_tt(wt);
	veh_year[ tt][0][ATV_NON_FINANCIAL_ASSETS] += delta;
	veh_month[tt][0][ATV_NON_FINANCIAL_ASSETS] += delta;
	veh_year[ TT_ALL][0][ATV_NON_FINANCIAL_ASSETS] += delta;
	veh_month[TT_ALL][0][ATV_NON_FINANCIAL_ASSETS] += delta;

	com_year[ 0][ATC_NETWEALTH] += delta;
	com_month[0][ATC_NETWEALTH] += delta;
}


transport_type finance_t::translate_waytype_to_tt(const waytype_t wt)
{
	switch(wt){
		case road_wt:      return TT_ROAD;
		case track_wt:     return TT_RAILWAY;
		case water_wt:     return TT_SHIP;
		case monorail_wt:  return TT_MONORAIL;
		case maglev_wt:    return TT_MAGLEV;
		case tram_wt:      return TT_TRAM;
		case narrowgauge_wt: return TT_NARROWGAUGE;
		case air_wt:       return TT_AIR;
		case powerline_wt: return TT_POWERLINE;
		case ignore_wt:
		case overheadlines_wt:
		default:           return TT_OTHER;
	}
}

waytype_t finance_t::translate_tt_to_waytype(const transport_type tt)
{
	switch (tt)
	{
		case TT_ROAD:			return road_wt;
		case TT_RAILWAY:		return track_wt;
		case TT_SHIP:			return water_wt;
		case TT_MONORAIL:		return monorail_wt;
		case TT_MAGLEV:			return maglev_wt;
		case  TT_TRAM:			return tram_wt;
		case TT_NARROWGAUGE:	return narrowgauge_wt;
		case  TT_AIR:			return air_wt;
		case TT_POWERLINE:		return powerline_wt;
		case TT_OTHER:			return overheadlines_wt;
		default:				return ignore_wt;
	}
}


/** compatibility code follows **/

#define OLD_MAX_PLAYER_HISTORY_YEARS  (12) // number of years to keep history
#define OLD_MAX_PLAYER_HISTORY_MONTHS  (12) // number of months to keep history

enum player_cost {
	COST_CONSTRUCTION=0,     // Construction
	COST_VEHICLE_RUN,        // Vehicle running costs
	COST_NEW_VEHICLE,        // New vehicles
	COST_INCOME,             // Income
	COST_MAINTENANCE,        // Upkeep
	COST_ASSETS,             // value of all vehicles and buildings
	COST_CASH,               // Cash
	COST_NETWEALTH,          // Total Cash + Assets
	COST_PROFIT,             // COST_POWERLINES+COST_INCOME-(COST_CONSTRUCTION+COST_VEHICLE_RUN+COST_NEW_VEHICLE+COST_MAINTENANCE)
	COST_OPERATING_PROFIT,   // COST_POWERLINES+COST_INCOME-(COST_VEHICLE_RUN+COST_MAINTENANCE)+COST_INTEREST
	COST_MARGIN,             // COST_OPERATING_PROFIT/COST_INCOME
	COST_ALL_TRANSPORTED,    // all transported goods
	COST_POWERLINES,         // revenue from the power grid
	COST_TRANSPORTED_PAS,    // number of passengers that actually reached destination
	COST_TRANSPORTED_MAIL,
	COST_TRANSPORTED_GOOD,
	COST_ALL_CONVOIS,        // number of convois
	COST_SCENARIO_COMPLETED, // scenario success (only useful if there is one ... )
	COST_WAY_TOLLS,
	COST_INTEREST,		// From extended
	COST_CREDIT_LIMIT	// From extended
	// OLD_MAX_PLAYER_COST = 21
};


void finance_t::export_to_cost_month(sint64 finance_history_month[][OLD_MAX_PLAYER_COST])
{
	calc_finance_history();
	for(int i=0; i<OLD_MAX_PLAYER_HISTORY_MONTHS; ++i){
		finance_history_month[i][COST_CONSTRUCTION] = veh_month[TT_ALL][i][ATV_CONSTRUCTION_COST];
		finance_history_month[i][COST_VEHICLE_RUN]  = veh_month[TT_ALL][i][ATV_RUNNING_COST] + veh_month[TT_ALL][i][ATV_VEHICLE_MAINTENANCE];
		finance_history_month[i][COST_NEW_VEHICLE]  = veh_month[TT_ALL][i][ATV_NEW_VEHICLE];
		finance_history_month[i][COST_INCOME]       = veh_month[TT_ALL][i][ATV_REVENUE_TRANSPORT];
		finance_history_month[i][COST_MAINTENANCE]  = veh_month[TT_ALL][i][ATV_INFRASTRUCTURE_MAINTENANCE];
		finance_history_month[i][COST_ASSETS]       = veh_month[TT_ALL][i][ATV_NON_FINANCIAL_ASSETS];
		finance_history_month[i][COST_CASH]         = com_month[i][ATC_CASH];
		finance_history_month[i][COST_NETWEALTH]    = com_month[i][ATC_NETWEALTH];
		finance_history_month[i][COST_PROFIT]       = veh_month[TT_ALL][i][ATV_PROFIT];
		finance_history_month[i][COST_OPERATING_PROFIT] = veh_month[TT_ALL][i][ATV_OPERATING_PROFIT];
		finance_history_month[i][COST_MARGIN]           = veh_month[TT_ALL][i][ATV_PROFIT_MARGIN];
		finance_history_month[i][COST_ALL_TRANSPORTED]  = veh_month[TT_ALL][i][ATV_TRANSPORTED];
		finance_history_month[i][COST_POWERLINES]       = veh_month[TT_POWERLINE][i][ATV_REVENUE];
		finance_history_month[i][COST_TRANSPORTED_PAS]  = veh_month[TT_ALL][i][ATV_DELIVERED_PASSENGER];
		finance_history_month[i][COST_TRANSPORTED_MAIL] = veh_month[TT_ALL][i][ATV_DELIVERED_MAIL];
		finance_history_month[i][COST_TRANSPORTED_GOOD] = veh_month[TT_ALL][i][ATV_DELIVERED_PASSENGER];
		finance_history_month[i][COST_ALL_CONVOIS]      = com_month[i][ATC_ALL_CONVOIS];
		finance_history_month[i][COST_SCENARIO_COMPLETED] = com_month[i][ATC_SCENARIO_COMPLETED];
		finance_history_month[i][COST_WAY_TOLLS]        = veh_month[TT_ALL][i][ATV_WAY_TOLL];
		finance_history_month[i][COST_INTEREST]			= com_month[i][ATC_INTEREST];
		finance_history_month[i][COST_CREDIT_LIMIT]		= - com_month[i][ATC_SOFT_CREDIT_LIMIT]; // reversed sign
	}
}


void finance_t::export_to_cost_year( sint64 finance_history_year[][OLD_MAX_PLAYER_COST])
{
	calc_finance_history();
	for(int i=0; i<OLD_MAX_PLAYER_HISTORY_YEARS; ++i){
		finance_history_year[i][COST_CONSTRUCTION] = veh_year[TT_ALL][i][ATV_CONSTRUCTION_COST];
		finance_history_year[i][COST_VEHICLE_RUN]  = veh_year[TT_ALL][i][ATV_RUNNING_COST] + veh_month[TT_ALL][i][ATV_VEHICLE_MAINTENANCE];
		finance_history_year[i][COST_NEW_VEHICLE]  = veh_year[TT_ALL][i][ATV_NEW_VEHICLE];
		finance_history_year[i][COST_INCOME]       = veh_year[TT_ALL][i][ATV_REVENUE_TRANSPORT];
		finance_history_year[i][COST_MAINTENANCE]  = veh_year[TT_ALL][i][ATV_INFRASTRUCTURE_MAINTENANCE];
		finance_history_year[i][COST_ASSETS]       = veh_year[TT_ALL][i][ATV_NON_FINANCIAL_ASSETS];
		finance_history_year[i][COST_CASH]         = com_year[i][ATC_CASH];
		finance_history_year[i][COST_NETWEALTH]    = com_year[i][ATC_NETWEALTH];
		finance_history_year[i][COST_PROFIT]       = veh_year[TT_ALL][i][ATV_PROFIT];
		finance_history_year[i][COST_OPERATING_PROFIT] = veh_year[TT_ALL][i][ATV_OPERATING_PROFIT];
		finance_history_year[i][COST_MARGIN]           = veh_year[TT_ALL][i][ATV_PROFIT_MARGIN];
		finance_history_year[i][COST_ALL_TRANSPORTED]  = veh_year[TT_ALL][i][ATV_TRANSPORTED];
		finance_history_year[i][COST_POWERLINES]       = veh_year[TT_POWERLINE][i][ATV_REVENUE];
		finance_history_year[i][COST_TRANSPORTED_PAS]  = veh_year[TT_ALL][i][ATV_DELIVERED_PASSENGER];
		finance_history_year[i][COST_TRANSPORTED_MAIL] = veh_year[TT_ALL][i][ATV_DELIVERED_MAIL];
		finance_history_year[i][COST_TRANSPORTED_GOOD] = veh_year[TT_ALL][i][ATV_DELIVERED_GOOD];
		finance_history_year[i][COST_ALL_CONVOIS]      = com_year[i][ATC_ALL_CONVOIS];
		finance_history_year[i][COST_SCENARIO_COMPLETED] = com_year[i][ATC_SCENARIO_COMPLETED];
		finance_history_year[i][COST_WAY_TOLLS]        = veh_year[TT_ALL][i][ATV_WAY_TOLL];
		finance_history_year[i][COST_INTEREST]			= com_year[i][ATC_INTEREST];
		finance_history_year[i][COST_CREDIT_LIMIT]		= - com_year[i][ATC_SOFT_CREDIT_LIMIT]; // reversed sign
	}
}


void finance_t::import_from_cost_month(const sint64 finance_history_month[][OLD_MAX_PLAYER_COST])
{
	// does it need initial clean-up ? (= initialization)
	for(int i=0; i<OLD_MAX_PLAYER_HISTORY_MONTHS; ++i){
		veh_month[TT_OTHER][i][ATV_CONSTRUCTION_COST] = finance_history_month[i][COST_CONSTRUCTION];
		veh_month[TT_ALL  ][i][ATV_CONSTRUCTION_COST] = finance_history_month[i][COST_CONSTRUCTION];
		veh_month[TT_OTHER][i][ATV_RUNNING_COST]      = finance_history_month[i][COST_VEHICLE_RUN];
		veh_month[TT_ALL  ][i][ATV_RUNNING_COST]      = finance_history_month[i][COST_VEHICLE_RUN];
		veh_month[TT_OTHER][i][ATV_NEW_VEHICLE]       = finance_history_month[i][COST_NEW_VEHICLE];
		veh_month[TT_ALL  ][i][ATV_NEW_VEHICLE]       = finance_history_month[i][COST_NEW_VEHICLE];
		// he have to store it in _GOOD for not being override in calc_finance history() to 0
		veh_month[TT_OTHER][i][ATV_REVENUE_GOOD]      = finance_history_month[i][COST_INCOME];
		veh_month[TT_ALL  ][i][ATV_REVENUE_GOOD]      = finance_history_month[i][COST_INCOME];
		veh_month[TT_OTHER][i][ATV_REVENUE_TRANSPORT]      = finance_history_month[i][COST_INCOME];
		veh_month[TT_ALL  ][i][ATV_REVENUE_TRANSPORT]      = finance_history_month[i][COST_INCOME];
		veh_month[TT_OTHER][i][ATV_INFRASTRUCTURE_MAINTENANCE] = finance_history_month[i][COST_MAINTENANCE];
		veh_month[TT_ALL  ][i][ATV_INFRASTRUCTURE_MAINTENANCE] = finance_history_month[i][COST_MAINTENANCE];
		veh_month[TT_OTHER][i][ATV_NON_FINANCIAL_ASSETS] = finance_history_month[i][COST_ASSETS];
		veh_month[TT_ALL  ][i][ATV_NON_FINANCIAL_ASSETS] = finance_history_month[i][COST_ASSETS];
		com_month[i][ATC_CASH]                        = finance_history_month[i][COST_CASH];
		com_month[i][ATC_NETWEALTH]                   = finance_history_month[i][COST_NETWEALTH];
		veh_month[TT_OTHER][i][ATV_PROFIT]            = finance_history_month[i][COST_PROFIT];
		veh_month[TT_ALL  ][i][ATV_PROFIT]            = finance_history_month[i][COST_PROFIT];
		veh_month[TT_OTHER][i][ATV_OPERATING_PROFIT]  = finance_history_month[i][COST_OPERATING_PROFIT];
		veh_month[TT_ALL  ][i][ATV_OPERATING_PROFIT]  = finance_history_month[i][COST_OPERATING_PROFIT];
		veh_month[TT_ALL  ][i][ATV_PROFIT_MARGIN]     = finance_history_month[i][COST_MARGIN]; // this needs to be recalculate before usage
		veh_month[TT_OTHER][i][ATV_TRANSPORTED]       = finance_history_month[i][COST_ALL_TRANSPORTED];
		veh_month[TT_ALL  ][i][ATV_TRANSPORTED]       = finance_history_month[i][COST_ALL_TRANSPORTED];
		veh_month[TT_POWERLINE][i][ATV_REVENUE]       = finance_history_month[i][COST_POWERLINES];
		veh_month[TT_OTHER][i][ATV_DELIVERED_PASSENGER] = finance_history_month[i][COST_TRANSPORTED_PAS];
		veh_month[TT_ALL  ][i][ATV_DELIVERED_PASSENGER] = finance_history_month[i][COST_TRANSPORTED_PAS];
		veh_month[TT_OTHER][i][ATV_DELIVERED_MAIL]  = finance_history_month[i][COST_TRANSPORTED_MAIL];
		veh_month[TT_ALL  ][i][ATV_DELIVERED_MAIL]  = finance_history_month[i][COST_TRANSPORTED_MAIL];
		veh_month[TT_OTHER][i][ATV_DELIVERED_GOOD]  = finance_history_month[i][COST_TRANSPORTED_GOOD];
		veh_month[TT_ALL  ][i][ATV_DELIVERED_GOOD]  = finance_history_month[i][COST_TRANSPORTED_GOOD];
		com_month[i][ATC_ALL_CONVOIS]                 = finance_history_month[i][COST_ALL_CONVOIS];
		com_month[i][ATC_SCENARIO_COMPLETED]          = finance_history_month[i][COST_SCENARIO_COMPLETED];
		if(finance_history_month[i][COST_WAY_TOLLS] > 0 ){
			veh_month[TT_OTHER][i][ATV_TOLL_RECEIVED] = finance_history_month[i][COST_WAY_TOLLS];
			veh_month[TT_ALL  ][i][ATV_TOLL_RECEIVED] = finance_history_month[i][COST_WAY_TOLLS];
		}
		else{
			veh_month[TT_OTHER][i][ATV_TOLL_PAID] = finance_history_month[i][COST_WAY_TOLLS];
			veh_month[TT_ALL  ][i][ATV_TOLL_PAID] = finance_history_month[i][COST_WAY_TOLLS];
		}
		veh_month[TT_OTHER][i][ATV_WAY_TOLL] = finance_history_month[i][COST_WAY_TOLLS];
		veh_month[TT_ALL  ][i][ATV_WAY_TOLL] = finance_history_month[i][COST_WAY_TOLLS];
		com_month[i][ATC_INTEREST] = finance_history_month[i][COST_INTEREST];
		com_month[i][ATC_SOFT_CREDIT_LIMIT] = - finance_history_month[i][COST_CREDIT_LIMIT]; // reversed sign
		com_month[i][ATC_HARD_CREDIT_LIMIT] = - 2 * finance_history_month[i][COST_CREDIT_LIMIT]; // reversed sign, doubled
	}
}


void finance_t::import_from_cost_year( const sint64 finance_history_year[][OLD_MAX_PLAYER_COST])
{
	for(int i=0; i<OLD_MAX_PLAYER_HISTORY_YEARS; ++i){
		veh_year[TT_OTHER][i][ATV_CONSTRUCTION_COST] = finance_history_year[i][COST_CONSTRUCTION];
		veh_year[TT_ALL  ][i][ATV_CONSTRUCTION_COST] = finance_history_year[i][COST_CONSTRUCTION];
		veh_year[TT_OTHER][i][ATV_RUNNING_COST]      = finance_history_year[i][COST_VEHICLE_RUN];
		veh_year[TT_ALL  ][i][ATV_RUNNING_COST]      = finance_history_year[i][COST_VEHICLE_RUN];
		veh_year[TT_OTHER][i][ATV_NEW_VEHICLE]       = finance_history_year[i][COST_NEW_VEHICLE];
		veh_year[TT_ALL  ][i][ATV_NEW_VEHICLE]       = finance_history_year[i][COST_NEW_VEHICLE];
		// we have to store it in _GOOD for not being override in calc_finance history() to 0
		veh_year[TT_OTHER][i][ATV_REVENUE_GOOD]      = finance_history_year[i][COST_INCOME];
		veh_year[TT_ALL  ][i][ATV_REVENUE_GOOD]      = finance_history_year[i][COST_INCOME];
		veh_year[TT_OTHER][i][ATV_REVENUE_TRANSPORT]      = finance_history_year[i][COST_INCOME];
		veh_year[TT_ALL  ][i][ATV_REVENUE_TRANSPORT]      = finance_history_year[i][COST_INCOME];
		veh_year[TT_OTHER][i][ATV_INFRASTRUCTURE_MAINTENANCE] = finance_history_year[i][COST_MAINTENANCE];
		veh_year[TT_ALL  ][i][ATV_INFRASTRUCTURE_MAINTENANCE] = finance_history_year[i][COST_MAINTENANCE];
		veh_year[TT_OTHER][i][ATV_NON_FINANCIAL_ASSETS] = finance_history_year[i][COST_ASSETS];
		veh_year[TT_ALL  ][i][ATV_NON_FINANCIAL_ASSETS] = finance_history_year[i][COST_ASSETS];
		com_year[i][ATC_CASH]                        = finance_history_year[i][COST_CASH];
		com_year[i][ATC_NETWEALTH]                   = finance_history_year[i][COST_NETWEALTH];
		veh_year[TT_OTHER][i][ATV_PROFIT]            = finance_history_year[i][COST_PROFIT];
		veh_year[TT_ALL  ][i][ATV_PROFIT]            = finance_history_year[i][COST_PROFIT];
		veh_year[TT_OTHER][i][ATV_OPERATING_PROFIT]  = finance_history_year[i][COST_OPERATING_PROFIT];
		veh_year[TT_ALL  ][i][ATV_OPERATING_PROFIT]  = finance_history_year[i][COST_OPERATING_PROFIT];
		veh_year[TT_ALL  ][i][ATV_PROFIT_MARGIN]     = finance_history_year[i][COST_MARGIN]; // this needs to be recalculate before usage
		// we have to store it in ATV_TRANSPORTED_GOOD, otherwise calc_finance_history will set ATV_TRANSPORTED to 0
		veh_year[TT_OTHER][i][ATV_TRANSPORTED_GOOD]  = finance_history_year[i][COST_ALL_TRANSPORTED];
		veh_year[TT_ALL  ][i][ATV_TRANSPORTED_GOOD]  = finance_history_year[i][COST_ALL_TRANSPORTED];
		veh_year[TT_OTHER][i][ATV_TRANSPORTED]       = finance_history_year[i][COST_ALL_TRANSPORTED];
		veh_year[TT_ALL  ][i][ATV_TRANSPORTED]       = finance_history_year[i][COST_ALL_TRANSPORTED];
		veh_year[TT_POWERLINE][i][ATV_REVENUE]       = finance_history_year[i][COST_POWERLINES];
		veh_year[TT_OTHER][i][ATV_DELIVERED_PASSENGER] = finance_history_year[i][COST_TRANSPORTED_PAS];
		veh_year[TT_ALL  ][i][ATV_DELIVERED_PASSENGER] = finance_history_year[i][COST_TRANSPORTED_PAS];
		veh_year[TT_OTHER][i][ATV_DELIVERED_MAIL]    = finance_history_year[i][COST_TRANSPORTED_MAIL];
		veh_year[TT_ALL  ][i][ATV_DELIVERED_MAIL]    = finance_history_year[i][COST_TRANSPORTED_MAIL];
		veh_year[TT_OTHER][i][ATV_DELIVERED_GOOD]    = finance_history_year[i][COST_TRANSPORTED_GOOD];
		veh_year[TT_ALL  ][i][ATV_DELIVERED_GOOD]    = finance_history_year[i][COST_TRANSPORTED_GOOD];
		com_year[i][ATC_ALL_CONVOIS]                 = finance_history_year[i][COST_ALL_CONVOIS];
		com_year[i][ATC_SCENARIO_COMPLETED]          = finance_history_year[i][COST_SCENARIO_COMPLETED];
		if(finance_history_year[i][COST_WAY_TOLLS] > 0 ){
			veh_year[TT_OTHER][i][ATV_TOLL_RECEIVED] = finance_history_year[i][COST_WAY_TOLLS];
			veh_year[TT_ALL  ][i][ATV_TOLL_RECEIVED] = finance_history_year[i][COST_WAY_TOLLS];
		}
		else{
			veh_year[TT_OTHER][i][ATV_TOLL_PAID] = finance_history_year[i][COST_WAY_TOLLS];
			veh_year[TT_ALL  ][i][ATV_TOLL_PAID] = finance_history_year[i][COST_WAY_TOLLS];
		}
		veh_year[TT_OTHER][i][ATV_WAY_TOLL] = finance_history_year[i][COST_WAY_TOLLS];
		veh_year[TT_ALL  ][i][ATV_WAY_TOLL] = finance_history_year[i][COST_WAY_TOLLS];
		com_year[i][ATC_INTEREST] = finance_history_year[i][COST_INTEREST];
		com_year[i][ATC_SOFT_CREDIT_LIMIT] = - finance_history_year[i][COST_CREDIT_LIMIT]; // reversed sign
		com_year[i][ATC_HARD_CREDIT_LIMIT] = - 2 * finance_history_year[i][COST_CREDIT_LIMIT]; // reversed sign, doubled
	}
}


void finance_t::rdwr_compatibility(loadsave_t *file)
{
	sint64 finance_history_year[OLD_MAX_PLAYER_HISTORY_YEARS][OLD_MAX_PLAYER_COST];
	sint64 finance_history_month[OLD_MAX_PLAYER_HISTORY_MONTHS][OLD_MAX_PLAYER_COST];

	for (int year=0; year<OLD_MAX_PLAYER_HISTORY_YEARS; year++) {
		for (int cost_type=0; cost_type<OLD_MAX_PLAYER_COST; cost_type++) {
			finance_history_year[year][cost_type] = 0;
			if ((cost_type == COST_CASH) || (cost_type == COST_NETWEALTH)) {
				finance_history_year[year][cost_type] = get_starting_money();
			}
		}
	}

	for (int month=0; month<OLD_MAX_PLAYER_HISTORY_MONTHS; month++) {
		for (int cost_type=0; cost_type<OLD_MAX_PLAYER_COST; cost_type++) {
			finance_history_month[month][cost_type] = 0;
			if ((cost_type == COST_CASH) || (cost_type == COST_NETWEALTH)) {
				finance_history_month[month][cost_type] = get_starting_money();
			}
		}
	}

	if( ( file->is_version_less(112, 5) ) && ( ! file->is_loading() ) ) { // for saving of game in old format
		export_to_cost_month( finance_history_month );
		export_to_cost_year( finance_history_year );
	}
	if (file->is_version_less(84, 8)) {
		// not so old save game
		for (int year = 0;year<OLD_MAX_PLAYER_HISTORY_YEARS;year++) {
			for (int cost_type = 0; cost_type<OLD_MAX_PLAYER_COST; cost_type++) {
				if (file->get_version_int() < 84007) {
					// a cost_type has has been added. For old savegames we only have 9 cost_types, now we have 10.
					// for old savegames only load 9 types and calculate the 10th; for new savegames load all 10 values
					if (cost_type < 9) {
						file->rdwr_longlong(finance_history_year[year][cost_type]);
					}
				} else {
					if (cost_type < 10) {
						file->rdwr_longlong(finance_history_year[year][cost_type]);
					}
				}
			}
		}
	}
	else if (file->is_version_less(86, 0)) {
		for (int year = 0;year<OLD_MAX_PLAYER_HISTORY_YEARS;year++) {
			for (int cost_type = 0; cost_type<10; cost_type++) {
				file->rdwr_longlong(finance_history_year[year][cost_type]);
			}
		}
		// in 84008 monthly finance history was introduced
		for (int month = 0;month<OLD_MAX_PLAYER_HISTORY_MONTHS;month++) {
			for (int cost_type = 0; cost_type<10; cost_type++) {
				file->rdwr_longlong(finance_history_month[month][cost_type]);
			}
		}
	}
	else if (file->is_version_less(99, 11)) {
		// powerline category missing
		for (int year = 0;year<OLD_MAX_PLAYER_HISTORY_YEARS;year++) {
			for (int cost_type = 0; cost_type<12; cost_type++) {
				file->rdwr_longlong(finance_history_year[year][cost_type]);
			}
		}
		for (int month = 0;month<OLD_MAX_PLAYER_HISTORY_MONTHS;month++) {
			for (int cost_type = 0; cost_type<12; cost_type++) {
				file->rdwr_longlong(finance_history_month[month][cost_type]);
			}
		}
	}
	else if (file->is_version_less(99, 17)) {
		// without detailed goo statistics
		for (int year = 0;year<OLD_MAX_PLAYER_HISTORY_YEARS;year++) {
			for (int cost_type = 0; cost_type<13; cost_type++) {
				file->rdwr_longlong(finance_history_year[year][cost_type]);
			}
		}
		for (int month = 0;month<OLD_MAX_PLAYER_HISTORY_MONTHS;month++) {
			for (int cost_type = 0; cost_type<13; cost_type++) {
				file->rdwr_longlong(finance_history_month[month][cost_type]);
			}
		}
	}
	else if(  file->is_version_less(102, 3) && file->get_extended_version() <= 1  ) {
		// saved everything
		for (int year = 0;year<OLD_MAX_PLAYER_HISTORY_YEARS;year++) {
			for (int cost_type = 0; cost_type<18; cost_type++) {
				file->rdwr_longlong(finance_history_year[year][cost_type]);
			}
		}
		for (int month = 0;month<OLD_MAX_PLAYER_HISTORY_MONTHS;month++) {
			for (int cost_type = 0; cost_type<18; cost_type++) {
				file->rdwr_longlong(finance_history_month[month][cost_type]);
			}
		}
	}
	else if(  file->get_version_int()<=102002  ) {
		// saved everything
		// Extended had INTEREST, CREDIT_LIMIT
		for (int year = 0;year<OLD_MAX_PLAYER_HISTORY_YEARS;year++) {
			for (int cost_type = 0; cost_type<21; cost_type++) {
				if (cost_type != COST_WAY_TOLLS) {
					file->rdwr_longlong(finance_history_year[year][cost_type]);
				}
			}
		}
		for (int month = 0;month<OLD_MAX_PLAYER_HISTORY_MONTHS;month++) {
			for (int cost_type = 0; cost_type<21; cost_type++) {
				if (cost_type != COST_WAY_TOLLS) {
					file->rdwr_longlong(finance_history_month[month][cost_type]);
				}
			}
		}
	}
	else if(  file->is_version_less(110, 7)  && file->get_extended_version()==0  ) {
		// only save what is needed
		// no way tolls
		for(int year = 0;  year<OLD_MAX_PLAYER_HISTORY_YEARS;  year++  ) {
			for(  int cost_type = 0;   cost_type<18;   cost_type++  ) {
				if(  cost_type<COST_NETWEALTH  ||  cost_type>COST_MARGIN  ) {
					file->rdwr_longlong(finance_history_year[year][cost_type]);
				}
			}
		}
		for (int month = 0;month<OLD_MAX_PLAYER_HISTORY_MONTHS;month++) {
			for (int cost_type = 0; cost_type<18; cost_type++) {
				if(  cost_type<COST_NETWEALTH  ||  cost_type>COST_MARGIN  ) {
					file->rdwr_longlong(finance_history_month[month][cost_type]);
				}
			}
		}
	}
	/* Note that extended did not adopt way tolls until version 11
	 * As a result the logic for version <=110006 for extended can fall through to the
	 * logic for version <= 112004
	 */
	else if (  file->is_version_less(112, 5)  && file->get_extended_version() == 0  ) {
		// savegame version: now with toll
		for(int year = 0;  year<OLD_MAX_PLAYER_HISTORY_YEARS;  year++  ) {
			for(  int cost_type = 0;   cost_type<19;   cost_type++  ) {
				if(  cost_type<COST_NETWEALTH  ||  cost_type>COST_MARGIN  ) {
					file->rdwr_longlong(finance_history_year[year][cost_type]);
				}
			}
		}
		for (int month = 0;month<OLD_MAX_PLAYER_HISTORY_MONTHS;month++) {
			for (int cost_type = 0; cost_type<19; cost_type++) {
				if(  cost_type<COST_NETWEALTH  ||  cost_type>COST_MARGIN  ) {
					file->rdwr_longlong(finance_history_month[month][cost_type]);
				}
			}
		}
	}
	else if (  file->get_version_int() <= 112004  && file->get_extended_version() == 1  ) {
		// is this combination even possible?  I doubt it
		// no way tolls in extended despite being in standard
		// no interest or credit limit in extended
		for(int year = 0;  year<OLD_MAX_PLAYER_HISTORY_YEARS;  year++  ) {
			for(  int cost_type = 0;   cost_type<18;   cost_type++  ) {
				if(  cost_type<COST_NETWEALTH  ||  cost_type>COST_MARGIN  ) {
					file->rdwr_longlong(finance_history_year[year][cost_type]);
				}
			}
		}
		for (int month = 0;month<OLD_MAX_PLAYER_HISTORY_MONTHS;month++) {
			for (int cost_type = 0; cost_type<18; cost_type++) {
				if(  cost_type<COST_NETWEALTH  ||  cost_type>COST_MARGIN  ) {
					file->rdwr_longlong(finance_history_month[month][cost_type]);
				}
			}
		}
	}
	else if (  file->get_version_int() <= 112004 && file->get_extended_version() <= 10  ) {
		// Standard had way tolls, extended still didn't
		// Extended also had INTEREST, CREDIT_LIMIT
		for(int year = 0;  year<OLD_MAX_PLAYER_HISTORY_YEARS;  year++  ) {
			for(  int cost_type = 0;   cost_type<21;   cost_type++  ) {
				if(  cost_type<COST_NETWEALTH  ||  cost_type>COST_MARGIN  ) {
					if (cost_type != COST_WAY_TOLLS) {
						file->rdwr_longlong(finance_history_year[year][cost_type]);
					}
				}
			}
		}
		for (int month = 0;month<OLD_MAX_PLAYER_HISTORY_MONTHS;month++) {
			for (int cost_type = 0; cost_type<21; cost_type++) {
				if(  cost_type<COST_NETWEALTH  ||  cost_type>COST_MARGIN  ) {
					if (cost_type != COST_WAY_TOLLS) {
						file->rdwr_longlong(finance_history_month[month][cost_type]);
					}
				}
			}
		}
	}
	else if (  file->get_version_int() <= 112004  ) {
		// Extended version 11 with old save file format
		// May happen in files saved with some development versions
		// Extended has WAY_TOLLS, INTEREST, CREDIT_LIMIT
		for(int year = 0;  year<OLD_MAX_PLAYER_HISTORY_YEARS;  year++  ) {
			for(  int cost_type = 0;   cost_type<21;   cost_type++  ) {
				if(  cost_type<COST_NETWEALTH  ||  cost_type>COST_MARGIN  ) {
					file->rdwr_longlong(finance_history_year[year][cost_type]);
				}
			}
		}
		for (int month = 0;month<OLD_MAX_PLAYER_HISTORY_MONTHS;month++) {
			for (int cost_type = 0; cost_type<21; cost_type++) {
				if(  cost_type<COST_NETWEALTH  ||  cost_type>COST_MARGIN  ) {
					file->rdwr_longlong(finance_history_month[month][cost_type]);
				}
			}
		}
	}
	else if (  file->get_version_int() >= 112005  ) {
		// We should not get here in compatibility loading mode
		assert(false);
	}

	if(  file->is_version_atleast(102, 3) && file->get_extended_version() != 7  ) {
		file->rdwr_longlong(starting_money);
	}

	// we have to pay maintenance at the beginning of a month
	if(file->is_version_less(99, 18)  &&  file->is_loading()) {
		finance_history_month[0][COST_MAINTENANCE] -= finance_history_month[1][COST_MAINTENANCE];
		finance_history_year [0][COST_MAINTENANCE] -= finance_history_month[1][COST_MAINTENANCE];
		set_account_balance(get_account_balance() - finance_history_month[1][COST_MAINTENANCE]);
	}


	if(file->is_loading()) {

		/* prior versions calculated margin incorrectly.
		 * we also save only some values and recalculate all dependent ones
		 * (remember: negative costs are just saved as negative numbers!)
		 */
		for(  int year=0;  year<OLD_MAX_PLAYER_HISTORY_YEARS;  year++  ) {
			finance_history_year[year][COST_NETWEALTH] = finance_history_year[year][COST_CASH]+finance_history_year[year][COST_ASSETS];
			// only revenue minus running costs
			finance_history_year[year][COST_OPERATING_PROFIT] = finance_history_year[year][COST_INCOME] + finance_history_year[year][COST_POWERLINES] + finance_history_year[year][COST_VEHICLE_RUN] + finance_history_year[year][COST_MAINTENANCE] + finance_history_year[year][COST_WAY_TOLLS];

			// including also investments into vehicles/infrastructure
			finance_history_year[year][COST_PROFIT] = finance_history_year[year][COST_OPERATING_PROFIT]+finance_history_year[year][COST_CONSTRUCTION]+finance_history_year[year][COST_NEW_VEHICLE]+finance_history_year[year][COST_INTEREST];
			finance_history_year[year][COST_MARGIN] = calc_margin(finance_history_year[year][COST_OPERATING_PROFIT], finance_history_year[year][COST_INCOME]);
		}
		for(  int month=0;  month<OLD_MAX_PLAYER_HISTORY_MONTHS;  month++  ) {
			finance_history_month[month][COST_NETWEALTH] = finance_history_month[month][COST_CASH]+finance_history_month[month][COST_ASSETS];
			finance_history_month[month][COST_OPERATING_PROFIT] = finance_history_month[month][COST_INCOME] + finance_history_month[month][COST_POWERLINES] + finance_history_month[month][COST_VEHICLE_RUN] + finance_history_month[month][COST_MAINTENANCE] + finance_history_month[month][COST_WAY_TOLLS];
			finance_history_month[month][COST_PROFIT] = finance_history_month[month][COST_OPERATING_PROFIT]+finance_history_month[month][COST_CONSTRUCTION]+finance_history_month[month][COST_NEW_VEHICLE]+finance_history_month[month][COST_INTEREST];
			finance_history_month[month][COST_MARGIN] = calc_margin(finance_history_month[month][COST_OPERATING_PROFIT], finance_history_month[month][COST_INCOME]);
		}

		// now import the statistics in old format to the new one
		import_from_cost_month(finance_history_month);
		import_from_cost_year(finance_history_year);
	}
}
