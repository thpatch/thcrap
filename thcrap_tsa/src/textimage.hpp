/**
  * Touhou Community Reliant Automatic Patcher
  * Team Shanghai Alice support plugin
  *
  * ----
  *
  * Support for custom images in place of original (mostly text) sprites.
  *
  * This module allows replacing one sprite from the game with a set of
  * sprites from a custom image. Typically, this is used to replace sprites
  * that receive rendered text, but might also be used for other sprites.
  *
  * It works by overwriting the game's sprite specification structure and the
  * ANM script, and storing additional Direct3D texture in the game's usually
  * more than ample texture slots. To load textures, we directly leverage
  * D3DXCreateTextureFromFileInMemoryEx() which comes with every TSA game,
  * bypassing the ANM format and its game-specific implementations.
  *
  * Does not support alpha-analyzed sprite-level patching at the PNG file
  * level yet; the highest image in the stack will always be used in its
  * entirety, and sprites will be shown even if they are fully alpha-empty.
  */

/*
 * Initializes the text image module by recording the necessary pointers to
 * in-game data. Should be placed after the game has allocated and initialized
 * all of these (most notably Direct3D) and is ready to load textures.
 * Will only be called once.
 *
 * Own JSON parameters
 * -------------------
 *	[D3DXCreateTextureFromFileInMemoryEx]
 *		Pointer to the statically-linked copy of that function.
 *		TODO: Retrieve from the import descriptor for d3dx9_*.dll if not given
 *		Type: immediate
 *
 *	[pD3D]
 *		Type: immediate to IDirect3DDevice*
 *
 *	[TextureSlots]
 *		Pointer to the game's texture slot array.
 *		Type: immediate to array of IDirect3DTexture*
 *
 *	[SpriteSpecs]
 *		Pointer to the game's sprite spec array.
 *		Type: immediate to array of the game's sprite spec type
 *
 *	[SpriteScripts]
 *		Pointer to the game's sprite script pointer array.
 *		Type: immediate to array of... pointers
 *
 * Other breakpoints called
 * ------------------------
 *	BP_textimage_load
 */
extern "C" __declspec(dllexport) int BP_textimage_init(x86_reg_t *regs, json_t *bp_info);

/*
 * Defines and (re-)loads text images.
 *
 * Own JSON parameters
 * -------------------
 *	[images]
 *		Definitions for text images, using these formats:
 *
 *		To (re-)define new images or update existing ones:
 *		{
 *			// Slot that our sprite spec will be written into.
 *			// Value is a flexible array that can specify multiple images
 *			// in ascending order of priority, just like patch stacks.
 *			"<Sprite slot, 32-bit hex>": [{
 *				"filename": <string>, // Mandatory
 *				"texture_slot": <32-bit integer>, // Mandatory
 *
 *				"sprite_w": <32-bit integer greater than 0>, // Mandatory
 *				"sprite_h": <32-bit integer greater than 0>, // Mandatory
 *
 *				"script": <binary hack> // Optional
 *			}],
 *			...
 *		}
 *
 *		[texture_slot] should be originally empty, or at least unused during
 *		the time the text image is active.
 *
 *		[sprite_w] and [sprite_h] define the size of a single sprite on the
 *		text image. Multiple sprites are put into a single image by simply
 *		tiling them; the individual sprites are then accessed from left to
 *		right, and from top to bottom. The size of each text image must
 *		therefore be a multiple of this width and height.
 *
 *		If [script] is given, the entire script will be replaced by that
 *		given one.
 *
 *		To reload images that were previously defined:
 *		{
 *			"<slotstring>": true,
 *			...
 *		}
 *
 *		Type: object
 *
 *	[groups]
 *		Specifies groups of texture slots; associated text images will only be
 *		set together with those. Removes any previous group definitions.
 *		Type: array of arrays
 *
 * Other breakpoints called
 * ------------------------
 *	None
 */
extern "C" __declspec(dllexport) int BP_textimage_load(x86_reg_t *regs, json_t *bp_info);

/*
 * Replaces a sprite from the gamee with one from a text image.
 *
 * Own JSON parameters
 * -------------------
 *	[sprites]
 *		A mapping from image names to sprite numbers:
 *		{
 *			"<slotstring>": <sprite number as immediate>,
 *			...
 *		}
 *		A "slotstring" is either an image file name (for specifying one unique
 *		image), or a texture slot as a hex value (for specifying the active
 *		image with the highest priority in this slot).
 *
 *		Type: object
 *
 * Other breakpoints called
 * ------------------------
 *	None
 */
extern "C" __declspec(dllexport) int BP_textimage_set(x86_reg_t *regs, json_t *bp_info);

/*
 * Only executes the code cave if for every given image, a sprite has been
 * set via a previous `textimage_set` breakpoint.
 *
 * Own JSON parameters
 * -------------------
 *	[slots]
 *		Slotstrings. Must have been previously registered in a
 *		`textimage_load` breakpoint.
 *		Type: flexible array
 *
 * Other breakpoints called
 * ------------------------
 *	None
 */
extern "C" __declspec(dllexport) int BP_textimage_is_active(x86_reg_t *regs, json_t *bp_info);

extern "C" __declspec(dllexport) void textimage_mod_repatch(json_t *files_changed);
extern "C" __declspec(dllexport) void textimage_mod_exit();
