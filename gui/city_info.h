/*
 * This file is part of the Simutrans-Extended project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef GUI_CITY_INFO_H
#define GUI_CITY_INFO_H


#include "../simcity.h"
#include "simwin.h"

#include "gui_frame.h"
#include "components/gui_chart.h"
#include "components/gui_textinput.h"
#include "components/action_listener.h"
#include "components/gui_label.h"
#include "components/gui_button.h"
#include "components/gui_button_to_chart.h"
#include "components/gui_tab_panel.h"
#include "components/gui_scrollpane.h"
#include "components/gui_speedbar.h"

class stadt_t;
template <class T> class sparse_tpl;
class gui_city_minimap_t;

/**
 * Window containing information about a city.
 */
class city_info_t : public gui_frame_t, private action_listener_t
{
private:
	char name[256], old_name[256]; ///< Name and old name of the city.

	stadt_t *city;                 ///< City for which the information is displayed

	gui_textinput_t name_input;    ///< Input field where the name of the city can be changed
	button_t allow_growth;         ///< Checkbox to enable/disable city growth
	gui_label_buf_t lb_size, lb_buildings, lb_border, lb_powerdemand;

	gui_tab_panel_t year_month_tabs, tabs;
	gui_aligned_container_t container_chart, container_year, container_month;
	gui_chart_t chart, mchart;                ///< Year and month history charts

	gui_city_minimap_t *pax_map;

	gui_button_to_chart_array_t button_to_chart;

	/// Renames the city to the name given in the text input field
	void rename_city();

	/// Resets the value of the text input field to the name of the city,
	/// e.g. when losing focus
	void reset_city_name();

	void update_labels();

	// stats table
	gui_aligned_container_t cont_city_stats;
	gui_scrollpane_t scrolly_stats;
	gui_bandgraph_t transportation_last_year, transportation_this_year;
	int transportation_pas[6];
	// update trigger
	uint32 old_month = 0;
	int update_seed = 0;
	void update_stats();

	void init();
public:
	city_info_t(stadt_t *city = NULL);

	virtual ~city_info_t();

	const char *get_help_filename() const OVERRIDE { return "citywindow.txt"; }

	koord3d get_weltpos(bool) OVERRIDE;

	bool is_weltpos() OVERRIDE;

	void draw(scr_coord pos, scr_size size) OVERRIDE;

	bool action_triggered(gui_action_creator_t*, value_t) OVERRIDE;

	void map_rotate90( sint16 ) OVERRIDE;

	// since we need to update the city pointer when topped
	bool infowin_event(event_t const*) OVERRIDE;

	void update_data();

	/**
	 * Does this window need a min size button in the title bar?
	 * @return true if such a button is needed
	 */
	bool has_min_sizer() const OVERRIDE {return true;}

	void rdwr(loadsave_t *file) OVERRIDE;

	uint32 get_rdwr_id() OVERRIDE { return magic_city_info_t; }
};

#endif
