#include "bsp/board.h"
#include "tusb.h"
#include <stdbool.h>
#include "hidparser/hidparser.h"

#define INVALID_REPORT_ID -1
// means 1/X of half range of analog would be dead zone
#define DEAD_ZONE 4U

#define CONFIG_BUTTON_A     GAMEPAD_BUTTON_0
#define CONFIG_BUTTON_B     GAMEPAD_BUTTON_1

static const char *const BUTTON_NAMES[] = {"NONE", "UP", "RIGHT", "DOWN", "LEFT", "A", "B"};
//(hat format, 8 is released, 0=N, 1=NE, 2=E, 3=SE, 4=S, 5=SW, 6=W, 7=NW)
static const uint8_t HAT_SWITCH_TO_DIRECTION_BUTTONS[] = {0b0001, 0b0011, 0b0010, 0b0110, 0b0100, 0b1100, 0b1000, 0b1001, 0b0000};

volatile uint8_t gamepad_info;      // Host gamepad info

typedef union
{
	struct
	{
		bool up : 1;
		bool right : 1;
		bool down : 1;
		bool left : 1;
		bool button1 : 1;
		bool button2 : 1;
	};
	struct
	{
		uint8_t all_direction : 4;
		uint8_t all_buttons : 2;
	};
	uint8_t value : 8;
} pad_buttons;

HID_ReportInfo_t *my_hid_info[4] = {NULL,NULL,NULL,NULL};
int16_t reportID;
pad_buttons previous = {0};
uint8_t g_dev_addr = 255;
uint8_t g_instance = 0;

//called from parser for filtering report items
bool CALLBACK_HIDParser_FilterHIDReportItem(HID_ReportItem_t *const CurrentItem)
{
	if (CurrentItem->ItemType != HID_REPORT_ITEM_In)
		return false;

	if (reportID == INVALID_REPORT_ID)
	{
		reportID = CurrentItem->ReportID;
	}
	switch (CurrentItem->Attributes.Usage.Page)
	{
		case HID_USAGE_PAGE_DESKTOP:
			switch (CurrentItem->Attributes.Usage.Usage)
			{
				case HID_USAGE_DESKTOP_X:
				case HID_USAGE_DESKTOP_Y:
				case HID_USAGE_DESKTOP_HAT_SWITCH:
				case HID_USAGE_DESKTOP_DPAD_UP:
				case HID_USAGE_DESKTOP_DPAD_DOWN:
				case HID_USAGE_DESKTOP_DPAD_LEFT:
				case HID_USAGE_DESKTOP_DPAD_RIGHT:
					return true;
			}
			return false;
		case HID_USAGE_PAGE_BUTTON:
			return true;
	}
	return false;
}

static inline bool USB_GetHIDReportItemInfoWithReportId(const uint8_t *ReportData, HID_ReportItem_t *const ReportItem)
{
	if (ReportItem->ReportID)
	{
		if (ReportItem->ReportID != ReportData[0])
			return false;

		ReportData++;
	}
	return USB_GetHIDReportItemInfo(ReportItem->ReportID, ReportData, ReportItem);
}

void parse_gamepad_report(uint8_t const *report, uint16_t len , uint8_t instance)
{
	pad_buttons current = {0};
	current.value = 0;
	HID_ReportItem_t *item = my_hid_info[instance]->FirstReportItem;  // MUST CHANGE to instance id 
	//iterate filtered reports info to match report from data
	while (item)
	{
		if (USB_GetHIDReportItemInfoWithReportId(report, item))
		{
			switch (item->Attributes.Usage.Page)
			{
			case HID_USAGE_PAGE_DESKTOP:
				switch (item->Attributes.Usage.Usage)
				{
				case HID_USAGE_DESKTOP_X:
				{
					uint32_t range_half = (item->Attributes.Logical.Maximum - item->Attributes.Logical.Minimum) / 2;
					uint32_t dead_zone_range = range_half / DEAD_ZONE;
					if (item->Value < (range_half - dead_zone_range))
					{
						current.left |= 1;
					}
					else if (item->Value > (range_half + dead_zone_range))
					{
						current.right |= 1;
					}
				}
				break;
				case HID_USAGE_DESKTOP_Y:
				{
					uint32_t range_half = (item->Attributes.Logical.Maximum - item->Attributes.Logical.Minimum) / 2;
					uint32_t dead_zone_range = range_half / DEAD_ZONE;
					if (item->Value < (range_half - dead_zone_range))
					{
						current.up |= 1;
					}
					else if (item->Value > (range_half + dead_zone_range))
					{
						current.down |= 1;
					}
				}
				break;
				case HID_USAGE_DESKTOP_HAT_SWITCH:
					current.all_direction |= HAT_SWITCH_TO_DIRECTION_BUTTONS[item->Value];
					break;
				case HID_USAGE_DESKTOP_DPAD_UP:
					current.up |= 1;
					break;
				case HID_USAGE_DESKTOP_DPAD_RIGHT:
					current.right |= 1;
					break;
				case HID_USAGE_DESKTOP_DPAD_DOWN:
					current.down |= 1;
					break;
				case HID_USAGE_DESKTOP_DPAD_LEFT:
					current.left |= 1;
					break;
				}
				break;
			case HID_USAGE_PAGE_BUTTON:
			{
				uint8_t usage = item->Attributes.Usage.Usage;
				if (usage == CONFIG_BUTTON_A)
				{
					if (item->Value)
					{
						current.button1 = 1;
					}
				}
				else if (usage == CONFIG_BUTTON_B)
				{
					if (item->Value)
					{
						current.button2 = 1;
					}
				}
			}
			break;
			}
		}
		item = item->Next;
	}
	if (previous.value != current.value)
	{
		//of course you can use GPIO here for hiven buttons
		uint8_t value = current.value;

        gamepad_info=0xff;

        if(value&32) {gamepad_info&=0xdf;};   // TRIG B
        if(value&16) {gamepad_info&=0xef;};   // TRIG A
        if(value&8) {gamepad_info&=0xfb;};   // Left
        if(value&4) {gamepad_info&=0xfd;};   // Down
        if(value&2) {gamepad_info&=0xf7;};  // Right
        if(value&1) {gamepad_info&=0xfe;};  // Up

		previous.value = current.value;
	}
}

