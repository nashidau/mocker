/**
 * Standard template for items so far is:
 * 	64 bit id.
 * 	First 4 bits idicate type of item.
 */

#ifndef ITEMS_H
#define ITEMS_H 1

#include <stdbool.h>

#include "anctypes.h"
//#include "character.h"
#include "util/idstr.h"

enum { ITEM_INVALID = (item_t)-1 };

enum {
        ITEM_FLAG_CAN_USE =     (1 << 0), /* Set the use flag */
        ITEM_FLAG_QUEST =       (1 << 1), /* Can't be sold, stolen or damaged */
        ITEM_FLAG_ORB =         (1 << 2), /* Can't be stolen, powerful */

        ITEM_FLAG_FOOD =        (1 <<  4), /* You can eat it */
        ITEM_FLAG_BULKY =       (1 <<  5), /* It's heavy;
                                              you can only carry one */

        /* Destroy Item flags */
        ITEM_FLAG_FRAGILE =     (1 << 10), /* Easy to break */
        ITEM_FLAG_PERMEABLE =   (1 << 11), /* Can be damaged by water/liquid */
        ITEM_FLAG_MELTS =       (1 << 12), /* Can be damaged by heat */
        ITEM_FLAG_FLAMMABLE =   (1 << 13), /* Can be damaged by fire */

        ITEM_FLAG_CLOTHING =    (1 << 18),

        ITEM_FLAG_WEAPON =      (1 << 20),
        ITEM_FLAG_SHIELD =      (1 << 21),

        ITEM_FLAG_CONTAINER =   (1 << 30),
};

enum item_shop_flags{
	ITEM_STORE_NOBUY =	(1 << 0), /**< Store won't buy this */
	ITEM_STORE_NOSELL =	(1 << 1), /**< Store won't sell this */
};

struct inv_item {
	item_t id;
	int count;
	/* may be null: cache essentially */
	struct item *item;
};

struct item_shop {
	uint32_t id;
	uint32_t shop;

	item_t itemid;
	struct item *item;
	struct qdb *qdb;

	uint32_t count;

	uint32_t min,max,delta;
	uint32_t discount;
	uint32_t flags;
};

struct item {
	const struct itemmethods *M;
        item_t id;
        uint32_t flags;//?? maybe should be a call, or set of calls
	const char *idstr;
};

struct itemdmg {
	int ndice;
	int sides;
	int bonus;
};

enum character_equip_slot {
	CH_EQUIP_FOOT,
	CH_EQUIP_HEAD,
	CH_EQUIP_HANDS,
	CH_EQUIP_HAND_PRIMARY,
	CH_EQUIP_LEGS,
};

struct itemmethods {
	/* Free an item */
	int (*free)(struct item *);
	/* Get the (display) name of hte object */
        const char *(*name)(struct item *);
	/* The image, if any */
        const char *(*image)(struct item *);
	/* The object description */
        const char *(*description)(struct item *);
	/* The price of the item */
	uint32_t (*price)(struct item *);
	/* Get the damage info of the item */
	const struct itemdmg *(*damage)(struct item *);
	/* The level of the item */
	int (*level)(struct item *);
	/* Can equip in this slot */
	int (*can_equip)(struct item *, enum character_equip_slot);
	/* What type of material is use to make this:
	 *	- Metal, wood or leather */
	// FIXME: Should this be an enum
	int (*construction_type)(struct item *);
	/* Get the item id as a string */
	const char *(*idstr)(struct item *);
};

struct item *item_get(struct qdb *qdb, item_t itemid);

int item_price_get(struct item *item);
int item_level_get(struct item *item);
int item_can_equip(struct item *item, enum character_equip_slot);
int item_is_armor(struct item *item);
int item_free(struct item *item);
const char *item_name_get(struct item *item);
const struct itemdmg *item_damage_get(struct item *item);

/* Generic functions for use in item implementations */
int item_generic_true(struct item *item);
int item_generic_false(struct item *item);
const char *item_generic_idstr(struct item *item);

#endif // ITEMS_H
