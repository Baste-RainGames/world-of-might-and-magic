#include "NuklearLegacyBindings.h"

#include <vector>
#include <cstdio>

#include <lua.hpp>

#include "Engine/AssetsManager.h"
#include "Engine/Graphics/Image.h"
#include "Engine/Graphics/Renderer/NuklearOverlayRenderer.h"
#include "Engine/Graphics/Renderer/Renderer.h"
#include "Library/Color/ColorTable.h"
#include "Library/Logger/Logger.h"

nk_context *NuklearLegacyBindings::_context{};
std::vector<struct lua_nk_style> NuklearLegacyBindings::_styles;

struct img {
    GraphicsImage *asset;
    struct nk_image nk;
};

struct context {
    std::vector<struct img *> imgs;
    std::vector<struct nk_tex_font *> fonts;
};

void NuklearLegacyBindings::setContext(nk_context *context) {
    _context = context;
}

#define lua_foreach(_lua, _idx) for (lua_pushnil(_lua); lua_next(_lua, _idx); lua_pop(_lua, 1))
#define lua_check_ret(_func) { int _ret = _func; if (_ret) return _ret; }

int lua_check_args_count(lua_State *L, bool condition) {
    if (!condition)
        return luaL_argerror(L, -2, lua_pushfstring(L, "invalid arguments count"));

    return 0;
}

int lua_check_args(lua_State *L, bool condition) {
    if (lua_gettop(L) == 0)
        return luaL_argerror(L, 1, lua_pushfstring(L, "context is absent"));

    if (!lua_isuserdata(L, 1))
        return luaL_argerror(L, 1, lua_pushfstring(L, "context is invalid"));

    return lua_check_args_count(L, condition);
}

bool lua_to_boolean(lua_State *L, int idx) {
    bool value{};
    if (lua_isnil(L, idx)) {
        return false;
    } else if (lua_isboolean(L, idx)) {
        value = lua_toboolean(L, idx);
    } else {
        std::string_view strVal = lua_tostring(L, idx);
        value = strVal != "false" && strVal != "0";
    }

    return value;
}

bool lua_error_check(lua_State *L, int err) {
    if (err != 0) {
        logger->error("Nuklear: LUA error: {}\n", lua_tostring(L, -1));
        lua_pop(L, 1);
        return true;
    }

    return false;
}

int NuklearLegacyBindings::lua_nk_parse_vec2(lua_State *L, int idx, struct nk_vec2 *vec) {
    if (lua_istable(L, idx)) {
        lua_pushvalue(L, idx);
        lua_pushnil(L);
        while (lua_next(L, -2)) {
            lua_pushvalue(L, -2);
            int i = luaL_checkinteger(L, -1);
            switch (i) {
            case(1):
                vec->x = (float)luaL_checknumber(L, -2);
                break;
            case(2):
                vec->y = (float)luaL_checknumber(L, -2);
                break;
            }
            lua_pop(L, 2);
        }

        lua_pop(L, 1);
    } else {
        return luaL_argerror(L, idx, lua_pushfstring(L, "vec2 format is wrong"));
    }

    return 0;
}

int NuklearLegacyBindings::lua_nk_is_hex(char c) {
    return (c >= '0' && c <= '9')
        || (c >= 'a' && c <= 'f')
        || (c >= 'A' && c <= 'F');
}

int NuklearLegacyBindings::lua_nk_parse_ratio(lua_State *L, int idx, std::vector<float> *ratio) {
    if (lua_istable(L, idx)) {
        lua_pushvalue(L, idx);
        lua_pushnil(L);
        int count = 0;
        while (lua_next(L, -2)) {
            ratio->push_back((float)luaL_checknumber(L, -1));
            count++;

            lua_pop(L, 1);
        }

        lua_pop(L, 1);
    } else {
        return luaL_argerror(L, idx, lua_pushfstring(L, "ratio format is wrong"));
    }

    return 0;
}

int NuklearLegacyBindings::lua_nk_parse_rect(lua_State *L, int idx, struct nk_rect *rect) {
    if (lua_istable(L, idx)) {
        lua_pushvalue(L, idx);
        lua_pushnil(L);
        while (lua_next(L, -2)) {
            lua_pushvalue(L, -2);
            if (lua_isnumber(L, -1)) {
                int i = luaL_checkinteger(L, -1);
                switch (i) {
                case(1):
                    rect->x = (float)luaL_checknumber(L, -2);
                    break;
                case(2):
                    rect->y = (float)luaL_checknumber(L, -2);
                    break;
                case(3):
                    rect->w = (float)luaL_checknumber(L, -2);
                    break;
                case(4):
                    rect->h = (float)luaL_checknumber(L, -2);
                    break;
                }
            } else {
                const char *key = lua_tostring(L, -1);
                float *val = &rect->x;
                if (!strcmp(key, "x")) {
                    val = &rect->x;
                } else if (!strcmp(key, "y")) {
                    val = &rect->y;
                } else if (!strcmp(key, "w")) {
                    val = &rect->w;
                } else if (!strcmp(key, "h")) {
                    val = &rect->h;
                }

                if (!val) {
                    return luaL_argerror(L, idx, lua_pushfstring(L, "rect format is wrong"));
                }
                *val = (float)luaL_checknumber(L, -2);
            }
            lua_pop(L, 2);
        }

        lua_pop(L, 1);
    } else {
        return luaL_argerror(L, idx, lua_pushfstring(L, "rect format is wrong"));
    }

    return 0;
}


int NuklearLegacyBindings::lua_nk_parse_align_header(lua_State *L, int idx, nk_flags *flag) {
    const char *strflag = luaL_checkstring(L, idx);

    if (!strcmp(strflag, "left"))
        *flag = NK_HEADER_LEFT;
    else if (!strcmp(strflag, "right"))
        *flag = NK_HEADER_RIGHT;
    else
        return luaL_argerror(L, idx, lua_pushfstring(L, "header align '%s' is unknown", strflag));

    return 0;
}

int NuklearLegacyBindings::lua_nk_parse_align_text(lua_State *L, int idx, nk_flags *flag) {
    const char *strflag = luaL_checkstring(L, idx);

    if (!strcmp(strflag, "align_bottom"))
        *flag = NK_TEXT_ALIGN_BOTTOM;
    else if (!strcmp(strflag, "align_centered"))
        *flag = NK_TEXT_ALIGN_CENTERED;
    else if (!strcmp(strflag, "align_left"))
        *flag = NK_TEXT_ALIGN_LEFT;
    else if (!strcmp(strflag, "align_middle"))
        *flag = NK_TEXT_ALIGN_MIDDLE;
    else if (!strcmp(strflag, "align_right"))
        *flag = NK_TEXT_ALIGN_RIGHT;
    else if (!strcmp(strflag, "align_top"))
        *flag = NK_TEXT_ALIGN_TOP;
    else if (!strcmp(strflag, "centered"))
        *flag = NK_TEXT_CENTERED;
    else if (!strcmp(strflag, "left"))
        *flag = NK_TEXT_LEFT;
    else if (!strcmp(strflag, "right"))
        *flag = NK_TEXT_RIGHT;
    else
        return luaL_argerror(L, idx, lua_pushfstring(L, "text align '%s' is unknown", strflag));

    return 0;
}

int NuklearLegacyBindings::lua_nk_parse_color(lua_State *L, int idx, nk_color *color, lua_nk_color_type type) {
    int r = -1, g = -1, b = -1, a = 255, h = -1, s = -1, v = -1;
    float f_r = -1, f_g = -1, f_b = -1, f_a = 255, f_h = -1, f_s = -1, f_v = -1;

    if (lua_istable(L, idx)) {
        lua_pushvalue(L, idx);
        lua_pushnil(L);
        while (lua_next(L, -2)) {
            lua_pushvalue(L, -2);
            if (lua_isnumber(L, -1)) {
                int i = luaL_checkinteger(L, -1);
                switch (i) {
                case(1):
                    r = luaL_checkinteger(L, -2);
                    break;
                case(2):
                    g = luaL_checkinteger(L, -2);
                    break;
                case(3):
                    b = luaL_checkinteger(L, -2);
                    break;
                case(4):
                    a = luaL_checkinteger(L, -2);
                    break;
                }
            } else {
                const char *key = luaL_checkstring(L, -1);

                if (!strcmp(key, "r")) {
                    r = luaL_checkinteger(L, -2);
                } else if (!strcmp(key, "g")) {
                    g = luaL_checkinteger(L, -2);
                } else if (!strcmp(key, "b")) {
                    b = luaL_checkinteger(L, -2);
                } else if (!strcmp(key, "a")) {
                    a = luaL_checkinteger(L, -2);
                } else if (!strcmp(key, "h")) {
                    h = luaL_checkinteger(L, -2);
                } else if (!strcmp(key, "s")) {
                    s = luaL_checkinteger(L, -2);
                } else if (!strcmp(key, "v")) {
                    v = luaL_checkinteger(L, -2);
                } else if (!strcmp(key, "fr")) {
                    f_r = (float)luaL_checknumber(L, -2);
                } else if (!strcmp(key, "fg")) {
                    f_g = (float)luaL_checknumber(L, -2);
                } else if (!strcmp(key, "fb")) {
                    f_b = (float)luaL_checknumber(L, -2);
                } else if (!strcmp(key, "fa")) {
                    f_a = (float)luaL_checknumber(L, -2);
                } else if (!strcmp(key, "fh")) {
                    f_h = (float)luaL_checknumber(L, -2);
                } else if (!strcmp(key, "fs")) {
                    f_s = (float)luaL_checknumber(L, -2);
                } else if (!strcmp(key, "fv")) {
                    f_v = (float)luaL_checknumber(L, -2);
                }
            }

            lua_pop(L, 2);
        }

        lua_pop(L, 1);

    } else if (lua_isstring(L, idx)) {
        bool is_hex_color = true;
        const char *strcolor = luaL_checkstring(L, idx);
        size_t len = strlen(strcolor);
        if ((len == 7 || len == 9) && strcolor[0] == '#') {
            for (size_t i = 1; i < len; ++i) {
                if (!lua_nk_is_hex(strcolor[i])) {
                    is_hex_color = false;
                    break;
                }
            }

            if (is_hex_color) {
                sscanf(strcolor, "#%02x%02x%02x", &r, &g, &b);

                if (len == 9)
                    sscanf(strcolor + 7, "%02x", &a);
            }
        }
    }

    switch (type) {
    case(LUA_COLOR_RGBA):
        if (r >= 0 && g >= 0 && b >= 0 && a >= 0 && r <= 255 && g <= 255 && b <= 255 && a <= 255) {
            *color = nk_rgba(r, g, b, a);
            return 0;
        }
        break;
    case(LUA_COLOR_RGBAF):
        if (f_r >= 0 && f_g >= 0 && f_b >= 0 && f_a >= 0 && f_r <= 255 && f_g <= 255 && f_b <= 255 && f_a <= 255) {
            *color = nk_rgba_f(f_r, f_g, f_b, f_a);
            return 0;
        }
        break;
    case(LUA_COLOR_HSVA):
        if (h >= 0 && s >= 0 && v >= 0 && a >= 0 && h <= 255 && s <= 255 && v <= 255 && a <= 255) {
            *color = nk_hsva(h, s, v, a);
            return 0;
        }
    case(LUA_COLOR_HSVAF):
        if (f_h >= 0 && f_s >= 0 && f_v >= 0 && f_a >= 0 && f_h <= 255 && f_s <= 255 && f_v <= 255 && f_a <= 255) {
            *color = nk_hsva_f(f_h, f_s, f_v, f_a);
            return 0;
        }
    default:
        if (r >= 0 && g >= 0 && b >= 0 && a >= 0 && r <= 255 && g <= 255 && b <= 255 && a <= 255) {
            *color = nk_rgba(r, g, b, a);
            return 0;
        } else if (h >= 0 && s >= 0 && v >= 0 && a >= 0 && h <= 255 && s <= 255 && v <= 255 && a <= 255) {
            *color = nk_hsva(h, s, v, a);
            return 0;
        } else if (f_r >= 0 && f_g >= 0 && f_b >= 0 && f_a >= 0 && f_r <= 255 && f_g <= 255 && f_b <= 255 && f_a <= 255) {
            *color = nk_rgba_f(f_r, f_g, f_b, f_a);
            return 0;
        } else if (f_h >= 0 && f_s >= 0 && f_v >= 0 && f_a >= 0 && f_h <= 255 && f_s <= 255 && f_v <= 255 && f_a <= 255) {
            *color = nk_hsva_f(f_h, f_s, f_v, f_a);
            return 0;
        }
        break;
    }

    return luaL_argerror(L, idx, lua_pushfstring(L, "unrecognized color format"));
}

int NuklearLegacyBindings::lua_nk_parse_color(lua_State *L, int idx, nk_color *color) {
    return lua_nk_parse_color(L, idx, color, LUA_COLOR_ANY);
}

void NuklearLegacyBindings::lua_nk_push_color(lua_State *L, nk_color color) {
    float f_r, f_g, f_b, f_a, f_h, f_s, f_v;
    int h, s, v;

    nk_color_f(&f_r, &f_g, &f_b, &f_a, color);
    nk_color_hsv_i(&h, &s, &v, color);
    nk_color_hsv_f(&f_h, &f_s, &f_v, color);

    lua_newtable(L);
    lua_pushliteral(L, "r");
    lua_pushinteger(L, color.r);
    lua_rawset(L, -3);
    lua_pushliteral(L, "g");
    lua_pushinteger(L, color.g);
    lua_rawset(L, -3);
    lua_pushliteral(L, "b");
    lua_pushinteger(L, color.b);
    lua_rawset(L, -3);
    lua_pushliteral(L, "a");
    lua_pushinteger(L, color.a);
    lua_rawset(L, -3);

    lua_pushliteral(L, "fr");
    lua_pushnumber(L, f_r);
    lua_rawset(L, -3);
    lua_pushliteral(L, "fg");
    lua_pushnumber(L, f_g);
    lua_rawset(L, -3);
    lua_pushliteral(L, "fb");
    lua_pushnumber(L, f_b);
    lua_rawset(L, -3);
    lua_pushliteral(L, "fa");
    lua_pushnumber(L, f_a);
    lua_rawset(L, -3);

    lua_pushliteral(L, "h");
    lua_pushinteger(L, h);
    lua_rawset(L, -3);
    lua_pushliteral(L, "s");
    lua_pushinteger(L, s);
    lua_rawset(L, -3);
    lua_pushliteral(L, "v");
    lua_pushinteger(L, v);
    lua_rawset(L, -3);

    lua_pushliteral(L, "fh");
    lua_pushnumber(L, f_h);
    lua_rawset(L, -3);
    lua_pushliteral(L, "fs");
    lua_pushnumber(L, f_s);
    lua_rawset(L, -3);
    lua_pushliteral(L, "fv");
    lua_pushnumber(L, f_v);
    lua_rawset(L, -3);
}

void NuklearLegacyBindings::lua_nk_push_colorf(lua_State *L, nk_colorf colorf) {
    nk_color color = nk_rgba_cf(colorf);

    lua_nk_push_color(L, color);
}

int NuklearLegacyBindings::lua_nk_parse_image(struct context *w, lua_State *L, int idx, struct nk_image **image) {
    size_t slot = luaL_checkinteger(L, idx);
    if (slot >= 0 && slot < w->imgs.size() && w->imgs[slot]->asset) {
        *image = &w->imgs[slot]->nk;
        return 0;
    }

    return luaL_argerror(L, idx, lua_pushfstring(L, "asset is wrong"));
}

int NuklearLegacyBindings::lua_nk_parse_image_asset(struct context *w, lua_State *L, int idx, GraphicsImage **asset) {
    size_t slot = luaL_checkinteger(L, idx);
    if (slot >= 0 && slot < w->imgs.size() && w->imgs[slot]->asset) {
        *asset = w->imgs[slot]->asset;
        return 0;
    }

    return luaL_argerror(L, idx, lua_pushfstring(L, "asset is wrong"));
}

int NuklearLegacyBindings::lua_nk_parse_layout_format(lua_State *L, int idx, nk_layout_format *fmt) {
    const char *strfmt = luaL_checkstring(L, idx);
    if (!strcmp(strfmt, "dynamic")) {
        *fmt = NK_DYNAMIC;
    } else if (!strcmp(strfmt, "static")) {
        *fmt = NK_STATIC;
    } else {
        return luaL_argerror(L, idx, lua_pushfstring(L, "unrecognized layout format '%s'", strfmt));
    }

    return 0;
}

int NuklearLegacyBindings::lua_nk_parse_modifiable(lua_State *L, int idx, nk_bool *modifiable) {
    const char *strmod = luaL_checkstring(L, idx);
    if (!strcmp(strmod, "modifiable")) {
        *modifiable = NK_MODIFIABLE;
    } else if (!strcmp(strmod, "fixed")) {
        *modifiable = NK_FIXED;
    } else {
        return luaL_argerror(L, idx, lua_pushfstring(L, "unrecognized modifiable '%s'", strmod));
    }

    return 0;
}

int NuklearLegacyBindings::lua_nk_parse_popup_type(lua_State *L, int idx, nk_popup_type *type) {
    const char *strtype = luaL_checkstring(L, idx);
    if (!strcmp(strtype, "dynamic")) {
        *type = NK_POPUP_DYNAMIC;
    } else if (!strcmp(strtype, "static")) {
        *type = NK_POPUP_STATIC;
    } else {
        return luaL_argerror(L, idx, lua_pushfstring(L, "unrecognized popup type '%s'", strtype));
    }

    return 0;
}

int NuklearLegacyBindings::lua_nk_parse_style(lua_State *L, int cidx, int pidx, lua_nk_style_type **type, void **ptr) {
    const char *component = luaL_checkstring(L, cidx);
    const char *property = luaL_checkstring(L, pidx);

    for (auto it = _styles.begin(); it < _styles.end(); it++) {
        struct lua_nk_style style = *it;
        // logger->Info("component: {}", style.component);
        if (!strcmp(component, style.component)) {
            for (auto itp = style.props.begin(); itp < style.props.end(); itp++) {
                struct lua_nk_property prop = *itp;
                // logger->Info("property: {}", prop.property);
                if (!strcmp(property, prop.property)) {
                    *type = &prop.type;
                    *ptr = prop.ptr;

                    return 0;
                }
            }

            return luaL_argerror(L, pidx, lua_pushfstring(L, "unknown property '%s'", property));
        }
    }

    return luaL_argerror(L, cidx, lua_pushfstring(L, "unknown component '%s", component));
}

#define PUSH_BUTTON_STYLE(element, style, property, type) element.push_back({ #property, &style->property, type});

int NuklearLegacyBindings::lua_nk_parse_style_button(struct context *w, lua_State *L, int idx, nk_style_button *style) {
    nk_color color;
    float value;
    struct nk_image *img;
    nk_style_item item;
    std::vector<struct lua_nk_property> buttons_styles;

    PUSH_BUTTON_STYLE(buttons_styles, style, active, lua_nk_style_type_item);
    PUSH_BUTTON_STYLE(buttons_styles, style, border_color, lua_nk_style_type_color);
    PUSH_BUTTON_STYLE(buttons_styles, style, hover, lua_nk_style_type_item);
    PUSH_BUTTON_STYLE(buttons_styles, style, normal, lua_nk_style_type_item);
    PUSH_BUTTON_STYLE(buttons_styles, style, text_active, lua_nk_style_type_color);
    PUSH_BUTTON_STYLE(buttons_styles, style, text_alignment, lua_nk_style_type_align_text);
    PUSH_BUTTON_STYLE(buttons_styles, style, text_background, lua_nk_style_type_color);
    PUSH_BUTTON_STYLE(buttons_styles, style, text_hover, lua_nk_style_type_color);
    PUSH_BUTTON_STYLE(buttons_styles, style, text_normal, lua_nk_style_type_color);
    PUSH_BUTTON_STYLE(buttons_styles, style, border, lua_nk_style_type_float);
    PUSH_BUTTON_STYLE(buttons_styles, style, image_padding, lua_nk_style_type_vec2);
    PUSH_BUTTON_STYLE(buttons_styles, style, padding, lua_nk_style_type_vec2);
    PUSH_BUTTON_STYLE(buttons_styles, style, rounding, lua_nk_style_type_float);
    PUSH_BUTTON_STYLE(buttons_styles, style, touch_padding, lua_nk_style_type_vec2);

    // import current style for buttons from context
    for (auto it = _styles.begin(); it < _styles.end(); it++) {
        struct lua_nk_style s = *it;
        if (!strcmp(s.component, "button")) {
            for (auto itc = s.props.begin(); itc < s.props.end(); itc++) {
                struct lua_nk_property sprop = *itc;
                for (auto itb = buttons_styles.begin(); itb < buttons_styles.end(); itb++) {
                    struct lua_nk_property bprop = *itb;
                    if (!strcmp(sprop.property, bprop.property)) {
                        switch (sprop.type) {
                        case(lua_nk_style_type_align_text):
                            memcpy(bprop.ptr, sprop.ptr, sizeof(nk_flags));
                            break;
                        case(lua_nk_style_type_color):
                            memcpy(bprop.ptr, sprop.ptr, sizeof(nk_color));
                            break;
                        case(lua_nk_style_type_float):
                            memcpy(bprop.ptr, sprop.ptr, sizeof(float));
                            break;
                        case(lua_nk_style_type_item):
                            memcpy(bprop.ptr, sprop.ptr, sizeof(struct nk_style_item));
                            break;
                        case(lua_nk_style_type_vec2):
                            memcpy(bprop.ptr, sprop.ptr, sizeof(struct nk_vec2));
                            break;
                        default:
                            break;
                        }
                    }
                }
            }

            break;
        }
    }

    if (lua_istable(L, idx)) {
        lua_pushvalue(L, idx);
        lua_pushnil(L);
        while (lua_next(L, -2)) {
            const char *key = luaL_checkstring(L, -2);

            for (auto itp = buttons_styles.begin(); itp < buttons_styles.end(); itp++) {
                struct lua_nk_property prop = *itp;
                if (!strcmp(key, prop.property)) {
                    switch (prop.type) {
                    case(lua_nk_style_type_align_text):
                        lua_check_ret(lua_nk_parse_align_text(L, -1, (nk_flags *)prop.ptr));
                        break;
                    case(lua_nk_style_type_color):
                        lua_check_ret(lua_nk_parse_color(L, -1, (nk_color *)prop.ptr));
                        break;
                    case(lua_nk_style_type_float):
                        value = (float)luaL_checknumber(L, -1);
                        memcpy(prop.ptr, &value, sizeof(float));
                        break;
                    case(lua_nk_style_type_item):
                        if (lua_isnumber(L, -1)) {
                            lua_check_ret(lua_nk_parse_image(w, L, -1, &img));
                            item = nk_style_item_image(*img);
                            memcpy(prop.ptr, &item, sizeof(nk_style_item));
                        } else if (lua_istable(L, -1) || lua_isstring(L, -1)) {
                            lua_check_ret(lua_nk_parse_color(L, -1, &color));
                            item = nk_style_item_color(color);
                            memcpy(prop.ptr, &item, sizeof(nk_style_item));
                        } else {
                            // TODO: nine slice
                            return luaL_argerror(L, -1, lua_pushfstring(L, "not implemented yet"));
                        }
                        break;
                    case(lua_nk_style_type_vec2):
                        lua_check_ret(lua_nk_parse_vec2(L, -1, (struct nk_vec2 *)prop.ptr));
                        break;
                    default:
                        break;
                    }
                }
            }

            lua_pop(L, 1);
        }
    }

    return 0;
}

int NuklearLegacyBindings::lua_nk_parse_edit_string_options(lua_State *L, int idx, nk_flags *flags) {
    *flags = NK_EDIT_FIELD;

    if (lua_istable(L, idx)) {
        lua_foreach(L, idx) {
            const char *flag = luaL_checkstring(L, -1);
            if (!strcmp(flag, "multiline"))
                *flags |= NK_EDIT_MULTILINE;
            else if (!strcmp(flag, "no_cursor"))
                *flags |= NK_EDIT_NO_CURSOR;
            else if (!strcmp(flag, "commit_on_enter"))
                *flags |= NK_EDIT_SIG_ENTER;
            else if (!strcmp(flag, "selectable"))
                *flags |= NK_EDIT_SELECTABLE;
            else if (!strcmp(flag, "goto_end_on_activate"))
                *flags |= NK_EDIT_GOTO_END_ON_ACTIVATE;
            else
                return luaL_argerror(L, idx, lua_pushfstring(L, "unrecognized edit string option '%s'", flag));
        }
    } else {
        return luaL_argerror(L, idx, lua_pushfstring(L, "wrong edit string option argument"));
    }

    return 0;
}

int NuklearLegacyBindings::lua_nk_parse_window_flags(lua_State *L, int idx, nk_flags *flags) {
    *flags = NK_WINDOW_NO_SCROLLBAR;

    if (lua_istable(L, idx)) {
        lua_foreach(L, idx) {
            const char *flag = luaL_checkstring(L, -1);
            if (!strcmp(flag, "border"))
                *flags |= NK_WINDOW_BORDER;
            else if (!strcmp(flag, "movable"))
                *flags |= NK_WINDOW_MOVABLE;
            else if (!strcmp(flag, "scalable"))
                *flags |= NK_WINDOW_SCALABLE;
            else if (!strcmp(flag, "closable"))
                *flags |= NK_WINDOW_CLOSABLE;
            else if (!strcmp(flag, "minimizable"))
                *flags |= NK_WINDOW_MINIMIZABLE;
            else if (!strcmp(flag, "scrollbar"))
                *flags &= ~NK_WINDOW_NO_SCROLLBAR;
            else if (!strcmp(flag, "title"))
                *flags |= NK_WINDOW_TITLE;
            else if (!strcmp(flag, "scroll_auto_hide"))
                *flags |= NK_WINDOW_SCROLL_AUTO_HIDE;
            else if (!strcmp(flag, "background"))
                *flags |= NK_WINDOW_BACKGROUND;
            else if (!strcmp(flag, "scale_left"))
                *flags |= NK_WINDOW_SCALE_LEFT;
            else if (!strcmp(flag, "no_input"))
                *flags |= NK_WINDOW_NO_INPUT;
            else
                return luaL_argerror(L, idx, lua_pushfstring(L, "unrecognized window flag '%s'", flag));
        }
    } else {
        return luaL_argerror(L, idx, lua_pushfstring(L, "wrong window flag argument"));
    }

    return 0;
}

int NuklearLegacyBindings::lua_nk_parse_scroll(lua_State *L, int idx, nk_scroll *scroll) {
    if (lua_istable(L, idx)) {
        lua_pushvalue(L, idx);
        lua_pushnil(L);
        while (lua_next(L, -2)) {
            lua_pushvalue(L, -2);
            int i = luaL_checkinteger(L, -1);
            switch (i) {
            case(1):
                scroll->x = (float)luaL_checknumber(L, -2);
                break;
            case(2):
                scroll->y = (float)luaL_checknumber(L, -2);
                break;
            }
            lua_pop(L, 2);
        }

        lua_pop(L, 1);
    } else {
        return luaL_argerror(L, idx, lua_pushfstring(L, "scroll format is wrong"));
    }

    return 0;
}

int NuklearLegacyBindings::lua_nk_parse_symbol(lua_State *L, int idx, nk_symbol_type *symbol) {
    const char *strsymbol = luaL_checkstring(L, idx);
    if (!strcmp(strsymbol, "x"))
        *symbol = nk_symbol_type::NK_SYMBOL_X;
    else if (!strcmp(strsymbol, "underscore"))
        *symbol = nk_symbol_type::NK_SYMBOL_UNDERSCORE;
    else if (!strcmp(strsymbol, "circle_solid"))
        *symbol = nk_symbol_type::NK_SYMBOL_CIRCLE_SOLID;
    else if (!strcmp(strsymbol, "circle_outline"))
        *symbol = nk_symbol_type::NK_SYMBOL_CIRCLE_OUTLINE;
    else if (!strcmp(strsymbol, "rect_solid"))
        *symbol = nk_symbol_type::NK_SYMBOL_RECT_SOLID;
    else if (!strcmp(strsymbol, "rect_outline"))
        *symbol = nk_symbol_type::NK_SYMBOL_RECT_OUTLINE;
    else if (!strcmp(strsymbol, "triangle_up"))
        *symbol = nk_symbol_type::NK_SYMBOL_TRIANGLE_UP;
    else if (!strcmp(strsymbol, "triangle_down"))
        *symbol = nk_symbol_type::NK_SYMBOL_TRIANGLE_DOWN;
    else if (!strcmp(strsymbol, "triangle_left"))
        *symbol = nk_symbol_type::NK_SYMBOL_TRIANGLE_LEFT;
    else if (!strcmp(strsymbol, "triangle_right"))
        *symbol = nk_symbol_type::NK_SYMBOL_TRIANGLE_RIGHT;
    else if (!strcmp(strsymbol, "triangle_plus"))
        *symbol = nk_symbol_type::NK_SYMBOL_PLUS;
    else if (!strcmp(strsymbol, "triangle_minus"))
        *symbol = nk_symbol_type::NK_SYMBOL_MINUS;
    else if (!strcmp(strsymbol, "triangle_max"))
        *symbol = nk_symbol_type::NK_SYMBOL_MAX;
    else
        return luaL_argerror(L, idx, lua_pushfstring(L, "symbol '%s' is unknown", strsymbol));

    return 0;
}

int NuklearLegacyBindings::lua_nk_parse_collapse_states(lua_State *L, int idx, nk_collapse_states *states) {
    const char *strstate = luaL_checkstring(L, idx);

    if (!strcmp(strstate, "minimized"))
        *states = NK_MINIMIZED;
    else if (!strcmp(strstate, "maximized"))
        *states = NK_MAXIMIZED;
    else
        return luaL_argerror(L, idx, lua_pushfstring(L, "state '%s' is unknown", strstate));

    return 0;
}

int NuklearLegacyBindings::lua_nk_parse_tree_type(lua_State *L, int idx, nk_tree_type *type) {
    const char *strtype = luaL_checkstring(L, idx);

    if (!strcmp(strtype, "tab"))
        *type = NK_TREE_TAB;
    else if (!strcmp(strtype, "node"))
        *type = NK_TREE_NODE;
    else
        return luaL_argerror(L, idx, lua_pushfstring(L, "type '%s' is unknown", strtype));

    return 0;
}

nk_scroll *NuklearLegacyBindings::lua_nk_check_scroll(lua_State *L, int idx) {
    void *userdata = luaL_checkudata(L, idx, "nk_scroll_mt");
    luaL_argcheck(L, userdata != nullptr, idx, "nk_scroll value expected");
    return (nk_scroll *)userdata;
}

int NuklearLegacyBindings::lua_nk_begin(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) >= 4 && lua_gettop(L) <= 5));

    const char *name, *title = NULL;
    nk_flags flags = 0;
    struct nk_rect rect;
    name = luaL_checkstring(L, 2);
    lua_check_ret(lua_nk_parse_rect(L, 3, &rect));
    lua_check_ret(lua_nk_parse_window_flags(L, 4, &flags));
    if (lua_gettop(L) == 5)
        title = luaL_checkstring(L, 5);

    bool ret = nk_begin_titled(_context, name, title ? title : name, rect, flags);

    lua_pushboolean(L, ret);

    return 1;
}

int NuklearLegacyBindings::lua_nk_button_color(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) == 2));

    struct nk_color color;
    lua_check_ret(lua_nk_parse_color(L, 2, &color));

    bool ret = nk_button_color(_context, color);

    lua_pushboolean(L, ret);

    return 1;
}

int NuklearLegacyBindings::lua_nk_button_image(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) >= 2 && lua_gettop(L) <= 5 && lua_gettop(L) != 3));

    struct context *w = (struct context *)lua_touserdata(L, 1);
    struct nk_style_button style = {};
    bool styled = false;
    nk_flags flag = 0;
    struct nk_image *img;
    const char *label = NULL;

    luaL_checkinteger(L, 2);
    lua_check_ret(lua_nk_parse_image(w, L, 2, &img));

    if (lua_gettop(L) >= 4 && lua_isstring(L, 3)) {
        label = luaL_checkstring(L, 3);
        lua_check_ret(lua_nk_parse_align_text(L, 4, &flag));
    }

    if (lua_gettop(L) == 5) {
        lua_check_ret(lua_nk_parse_style_button(w, L, 5, &style));
        styled = true;
    }

    bool ret;
    if (label != NULL && styled)
        ret = nk_button_image_label_styled(_context, &style, *img, label, flag);
    else if (label != NULL)
        ret = nk_button_image_label(_context, *img, label, flag);
    else if (styled)
        ret = nk_button_image_styled(_context, &style, *img);
    else
        ret = nk_button_image(_context, *img);

    lua_pushboolean(L, ret);

    return 1;
}

int NuklearLegacyBindings::lua_nk_button_label(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) >= 2 && lua_gettop(L) <= 3));

    struct context *w = (struct context *)lua_touserdata(L, 1);
    struct nk_style_button style = {};
    const char *label = luaL_checkstring(L, 2);

    bool ret;
    if (lua_gettop(L) == 3) {
        lua_check_ret(lua_nk_parse_style_button(w, L, 3, &style));
        ret = nk_button_label_styled(_context, &style, label);
    } else {
        ret = nk_button_label(_context, label);
    }

    lua_pushboolean(L, ret);

    return 1;
}

int NuklearLegacyBindings::lua_nk_button_set_behavior(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) == 2));

    enum nk_button_behavior behaviour;
    const char *strbehaviour = luaL_checkstring(L, 2);
    if (!strcmp(strbehaviour, "default"))
        behaviour = NK_BUTTON_DEFAULT;
    else if (!strcmp(strbehaviour, "repeater"))
        behaviour = NK_BUTTON_REPEATER;
    else
        return luaL_argerror(L, 2, lua_pushfstring(L, "behaviour '%s' is unknown", strbehaviour));

    nk_button_set_behavior(_context, behaviour);

    return 0;
}

int NuklearLegacyBindings::lua_nk_button_symbol(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) >= 2 && lua_gettop(L) <= 5 && lua_gettop(L) != 3));

    struct context *w = (struct context *)lua_touserdata(L, 1);
    struct nk_style_button style = {};
    const char *label = NULL;
    nk_flags flag = 0;
    bool styled = false;
    nk_symbol_type symbol = nk_symbol_type::NK_SYMBOL_NONE;

    lua_check_ret(lua_nk_parse_symbol(L, 2, &symbol))

        if (lua_gettop(L) >= 4 && lua_isstring(L, 3)) {
            label = luaL_checkstring(L, 3);
            lua_check_ret(lua_nk_parse_align_text(L, 4, &flag));
        }

    if (lua_gettop(L) == 5) {
        lua_check_ret(lua_nk_parse_style_button(w, L, 5, &style));
        styled = true;
    }

    bool ret;
    if (label != NULL && styled)
        ret = nk_button_symbol_label_styled(_context, &style, symbol, label, flag);
    else if (label != NULL)
        ret = nk_button_symbol_label(_context, symbol, label, flag);
    else if (styled)
        ret = nk_button_symbol_styled(_context, &style, symbol);
    else
        ret = nk_button_symbol(_context, symbol);

    lua_pushboolean(L, ret);

    return 1;
}

int NuklearLegacyBindings::lua_nk_chart_begin(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) == 5));

    nk_chart_type type;
    const char *strtype = luaL_checkstring(L, 2);
    int num = luaL_checkinteger(L, 3);
    float min = (float)luaL_checknumber(L, 4);
    float max = (float)luaL_checknumber(L, 5);

    if (!strcmp(strtype, "lines"))
        type = NK_CHART_LINES;
    else if (!strcmp(strtype, "column"))
        type = NK_CHART_COLUMN;
    else if (!strcmp(strtype, "max"))
        type = NK_CHART_MAX;
    else
        return luaL_argerror(L, 2, lua_pushfstring(L, "type '%s' is unknown", strtype));

    bool ret = nk_chart_begin(_context, type, num, min, max);

    lua_pushboolean(L, ret);

    return 1;
}

int NuklearLegacyBindings::lua_nk_chart_end(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) == 1));

    nk_chart_end(_context);

    return 0;
}

int NuklearLegacyBindings::lua_nk_chart_push(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) == 2));

    float value = (float)luaL_checknumber(L, 2);

    nk_flags flags = nk_chart_push(_context, value);

    const char *str = "";
    if (flags & NK_CHART_CLICKED)
        str = "clicked";
    else if (flags & NK_CHART_HOVERING)
        str = "hovering";

    lua_pushstring(L, str);

    return 1;
}

int NuklearLegacyBindings::lua_nk_checkbox(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) == 3));

    const char *label = luaL_checkstring(L, 2);
    nk_bool value = lua_toboolean(L, 3);

    bool ret = nk_checkbox_label(_context, label, &value);
    lua_pushboolean(L, value);
    lua_pushboolean(L, ret);

    return 2;
}

int NuklearLegacyBindings::lua_nk_color_picker(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) == 3));

    nk_color color;
    nk_color_format format;
    const char *strformat = luaL_checkstring(L, 3);
    lua_check_ret(lua_nk_parse_color(L, 2, &color));

    if (!strcmp(strformat, "rgb"))
        format = NK_RGB;
    else if (!strcmp(strformat, "rgba"))
        format = NK_RGBA;
    else
        return luaL_argerror(L, 3, lua_pushfstring(L, "wrong color format"));

    nk_colorf colorf = nk_color_cf(color);
    colorf = nk_color_picker(_context, colorf, format);

    lua_nk_push_colorf(L, colorf);

    return 1;
}

int NuklearLegacyBindings::lua_nk_color_update(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) == 3));

    nk_color color;
    const char *strformat = luaL_checkstring(L, 3);

    if (!strcmp(strformat, "rgba")) {
        lua_check_ret(lua_nk_parse_color(L, 2, &color, LUA_COLOR_RGBA));
    } else if (!strcmp(strformat, "rgbaf")) {
        lua_check_ret(lua_nk_parse_color(L, 2, &color, LUA_COLOR_RGBAF));
    } else if (!strcmp(strformat, "hsva")) {
        lua_check_ret(lua_nk_parse_color(L, 2, &color, LUA_COLOR_HSVA));
    } else if (!strcmp(strformat, "hsvaf")) {
        lua_check_ret(lua_nk_parse_color(L, 2, &color, LUA_COLOR_HSVAF));
    } else {
        return luaL_argerror(L, 3, lua_pushfstring(L, "wrong color format"));
    }

    lua_nk_push_color(L, color);

    return 1;
}

int NuklearLegacyBindings::lua_nk_combo(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) == 5));

    struct context *w = (struct context *)lua_touserdata(L, 1);
    char **items;
    int count = 0;
    int selected = luaL_checkinteger(L, 3);
    int item_height = luaL_checkinteger(L, 4);
    struct nk_vec2 size;

    lua_check_ret(lua_nk_parse_vec2(L, 5, &size));

    if (lua_istable(L, 2)) {
        count = lua_objlen(L, 2);
        lua_pushvalue(L, 2);
        lua_pushnil(L);
        items = (char **)calloc(count, sizeof(char *));
        int i = 0;
        while (lua_next(L, -2)) {
            const char *val = luaL_checkstring(L, -1);

            items[i] = (char *)val;

            i++;
            lua_pop(L, 1);
        }

        assert(0); //this needs to be reimplemented in a different way
        //w->tmp.push_back((void *)items);
    } else {
        return luaL_argerror(L, 2, lua_pushfstring(L, "wrong items"));
    }

    int ret = nk_combo(_context, (const char **)items, count, selected, item_height, size);

    lua_pushinteger(L, ret);

    return 1;
}

int NuklearLegacyBindings::lua_nk_combo_begin_color(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) == 3));

    struct nk_color color;
    struct nk_vec2 size;

    lua_check_ret(lua_nk_parse_color(L, 2, &color));
    lua_check_ret(lua_nk_parse_vec2(L, 3, &size));

    bool ret = nk_combo_begin_color(_context, color, size);

    lua_pushboolean(L, ret);

    return 1;
}

int NuklearLegacyBindings::lua_nk_combo_begin_image(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) >= 3 && lua_gettop(L) <= 4));

    struct context *w = (struct context *)lua_touserdata(L, 1);
    struct nk_image *img;
    struct nk_vec2 size;
    const char *label = NULL;

    lua_check_ret(lua_nk_parse_image(w, L, 2, &img));
    lua_check_ret(lua_nk_parse_vec2(L, 3, &size));

    if (lua_gettop(L) == 4) {
        label = luaL_checkstring(L, 4);
    }

    bool ret;
    if (label)
        ret = nk_combo_begin_image_label(_context, label, *img, size);
    else
        ret = nk_combo_begin_image(_context, *img, size);

    lua_pushboolean(L, ret);

    return 1;
}

int NuklearLegacyBindings::lua_nk_combo_begin_label(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) == 3));

    struct nk_vec2 size;
    const char *label = luaL_checkstring(L, 2);
    lua_check_ret(lua_nk_parse_vec2(L, 3, &size));

    bool ret = nk_combo_begin_label(_context, label, size);

    lua_pushboolean(L, ret);

    return 1;
}

int NuklearLegacyBindings::lua_nk_combo_begin_symbol(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) >= 3 && lua_gettop(L) <= 4));

    nk_symbol_type symbol = nk_symbol_type::NK_SYMBOL_NONE;
    struct nk_vec2 size;
    const char *label = NULL;

    lua_check_ret(lua_nk_parse_symbol(L, 2, &symbol))
        lua_check_ret(lua_nk_parse_vec2(L, 3, &size));
    if (lua_gettop(L) == 4) {
        label = luaL_checkstring(L, 4);
    }

    bool ret;
    if (label)
        ret = nk_combo_begin_symbol_label(_context, label, symbol, size);
    else
        ret = nk_combo_begin_symbol(_context, symbol, size);

    lua_pushboolean(L, ret);

    return 1;
}

int NuklearLegacyBindings::lua_nk_combo_close(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) == 1));

    nk_combo_close(_context);

    return 0;
}

int NuklearLegacyBindings::lua_nk_combo_item_image(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) == 4));

    struct context *w = (struct context *)lua_touserdata(L, 1);
    struct nk_image *img;
    nk_flags flag = 0;
    const char *label = luaL_checkstring(L, 3);
    lua_check_ret(lua_nk_parse_image(w, L, 2, &img));
    lua_check_ret(lua_nk_parse_align_text(L, 4, &flag));

    bool ret = nk_combo_item_image_label(_context, *img, label, flag);

    lua_pushboolean(L, ret);

    return 1;
}

int NuklearLegacyBindings::lua_nk_image_dimensions(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) == 2));

    struct context *w = (struct context *)lua_touserdata(L, 1);
    GraphicsImage *asset = NULL;
    luaL_checkinteger(L, 2);

    lua_check_ret(lua_nk_parse_image_asset(w, L, 2, &asset));

    lua_pushnumber(L, asset->height());
    lua_pushnumber(L, asset->width());

    return 2;
}

int NuklearLegacyBindings::lua_nk_combo_item_label(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) == 3));

    nk_flags flag = 0;
    const char *label = luaL_checkstring(L, 2);
    lua_check_ret(lua_nk_parse_align_text(L, 3, &flag));

    bool ret = nk_combo_item_label(_context, label, flag);

    lua_pushboolean(L, ret);
    return 1;
}

int NuklearLegacyBindings::lua_nk_combo_item_symbol(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) == 4));

    nk_symbol_type symbol = nk_symbol_type::NK_SYMBOL_NONE;
    nk_flags flag = 0;
    const char *label = luaL_checkstring(L, 3);
    lua_check_ret(lua_nk_parse_symbol(L, 2, &symbol))
        lua_check_ret(lua_nk_parse_align_text(L, 4, &flag));

    bool ret = nk_combo_item_symbol_label(_context, symbol, label, flag);

    lua_pushboolean(L, ret);

    return 1;
}

int NuklearLegacyBindings::lua_nk_combo_end(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) == 1));

    nk_combo_end(_context);

    return 0;
}

void NuklearLegacyBindings::lua_nk_push_edit_string_result_flag(lua_State *L, nk_flags flags) {
    static const auto editStringResults = std::array{
        NK_EDIT_ACTIVE,
        NK_EDIT_INACTIVE,
        NK_EDIT_ACTIVATED,
        NK_EDIT_DEACTIVATED,
        NK_EDIT_COMMITED,
    };
    lua_newtable(L);
    for (int i = 0; i < editStringResults.size(); ++i) {
        const auto &flag = editStringResults[i];
        if (flags & flag) {
            lua_pushinteger(L, flag);
            lua_pushboolean(L, true);
            lua_rawset(L, -3);
        }
    }
}

int NuklearLegacyBindings::lua_nk_edit_string(lua_State *L) {
    /*
    * Very temp buffer solution, but easy to use on the lua side
    * edit_string is expecting a buffer which would require lua to provide
    * new ways to instantiate a buffer
    */
    static const int MAX_BUFFER_SIZE = 1024;
    static char buffer[MAX_BUFFER_SIZE] = { 0 };

    lua_check_ret(lua_check_args(L, lua_gettop(L) == 3));

    nk_flags flags = 0;
    lua_check_ret(lua_nk_parse_edit_string_options(L, 2, &flags));
    const char *text = lua_tostring(L, 3);
    size_t len = NK_CLAMP(0, strlen(text), MAX_BUFFER_SIZE - 1);
    memcpy(buffer, text, len);
    buffer[len] = '\0';
    int bufferLength = strlen(buffer);

    nk_flags resultFlags = nk_edit_string_zero_terminated(_context, flags, buffer, MAX_BUFFER_SIZE - 1, nk_filter_default);

    lua_pushstring(L, buffer);
    lua_nk_push_edit_string_result_flag(L, resultFlags);

    return 2;
}

int NuklearLegacyBindings::lua_nk_end(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) == 1));

    nk_end(_context);

    return 0;
}

int NuklearLegacyBindings::lua_nk_group_begin(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) == 3));

    const char *title = luaL_checkstring(L, 2);
    nk_flags flags;
    lua_check_ret(lua_nk_parse_window_flags(L, 3, &flags));

    bool ret = nk_group_begin(_context, title, flags);

    lua_pushboolean(L, ret);

    return 1;
}

int NuklearLegacyBindings::lua_nk_group_end(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) == 1));

    nk_group_end(_context);

    return 0;
}

int NuklearLegacyBindings::lua_nk_group_scrolled_begin(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) == 4));

    nk_scroll *scroll = lua_nk_check_scroll(L, 2);
    const char *title = luaL_checkstring(L, 3);
    nk_flags flags;
    lua_check_ret(lua_nk_parse_window_flags(L, 4, &flags));

    bool ret = nk_group_scrolled_begin(_context, scroll, title, flags);

    lua_pushboolean(L, ret);

    return 1;
}

int NuklearLegacyBindings::lua_nk_group_scrolled_end(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) == 1));

    nk_group_scrolled_end(_context);

    return 0;
}

int NuklearLegacyBindings::lua_nk_group_set_scroll(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) == 4));

    const char *id = luaL_checkstring(L, 2);
    size_t xoffset = luaL_checkinteger(L, 3);
    size_t yoffset = luaL_checkinteger(L, 4);
    nk_group_set_scroll(_context, id, xoffset, yoffset);

    return 0;
}

int NuklearLegacyBindings::lua_nk_layout_reset_min_row_height(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) == 1));
    nk_layout_reset_min_row_height(_context);
    return 0;
}

int NuklearLegacyBindings::lua_nk_label(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) >= 2 && lua_gettop(L) <= 3));

    nk_flags flag = 0;
    const char *label = luaL_checkstring(L, 2);
    if (lua_gettop(L) == 3)
        lua_check_ret(lua_nk_parse_align_text(L, 3, &flag));

    if (flag)
        nk_label(_context, label, flag);
    else
        nk_label_wrap(_context, label);

    return 0;
}

int NuklearLegacyBindings::lua_nk_label_colored(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) >= 3 && lua_gettop(L) <= 4));

    nk_flags flag = 0;
    nk_color color;
    const char *label = luaL_checkstring(L, 2);
    lua_check_ret(lua_nk_parse_color(L, 3, &color));
    if (lua_gettop(L) == 4)
        lua_check_ret(lua_nk_parse_align_text(L, 4, &flag));

    if (flag)
        nk_label_colored(_context, label, flag, color);
    else
        nk_label_colored_wrap(_context, label, color);

    return 0;
}

int NuklearLegacyBindings::lua_nk_layout_row_dynamic(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) == 3));

    float height = (float)luaL_checknumber(L, 2);
    int cols = luaL_checkinteger(L, 3);

    nk_layout_row_dynamic(_context, height, cols);

    return 0;
}

int NuklearLegacyBindings::lua_nk_layout_row_static(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) == 4));

    float height = (float)luaL_checknumber(L, 2);
    float width = (float)luaL_checknumber(L, 3);
    int cols = luaL_checkinteger(L, 4);

    nk_layout_row_static(_context, height, width, cols);

    return 0;
}

int NuklearLegacyBindings::lua_nk_layout_row_begin(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) == 4));

    enum nk_layout_format fmt;
    float height = (float)luaL_checknumber(L, 3);
    int cols = luaL_checkinteger(L, 4);
    lua_check_ret(lua_nk_parse_layout_format(L, 2, &fmt));

    nk_layout_row_begin(_context, fmt, height, cols);

    return 0;
}

int NuklearLegacyBindings::lua_nk_layout_row_push(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) == 2));

    float ratio = (float)luaL_checknumber(L, 2);

    nk_layout_row_push(_context, ratio);

    return 0;
}

int NuklearLegacyBindings::lua_nk_layout_row_end(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) == 1));

    nk_layout_row_end(_context);

    return 0;
}

int NuklearLegacyBindings::lua_nk_layout_space_begin(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) == 4));

    enum nk_layout_format fmt;
    float height = (float)luaL_checknumber(L, 3);
    int widget_count = luaL_checkinteger(L, 4);
    lua_check_ret(lua_nk_parse_layout_format(L, 2, &fmt));

    nk_layout_space_begin(_context, fmt, height, widget_count);

    return 0;
}

int NuklearLegacyBindings::lua_nk_layout_space_end(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) == 1));

    nk_layout_space_end(_context);

    return 0;
}

int NuklearLegacyBindings::lua_nk_layout_space_push(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) == 2));

    struct nk_rect rect;
    lua_check_ret(lua_nk_parse_rect(L, 2, &rect));

    nk_layout_space_push(_context, rect);

    return 0;
}

int NuklearLegacyBindings::lua_nk_style_default(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) == 1));

    nk_style_default(_context);

    return 0;
}

int NuklearLegacyBindings::lua_nk_menu_begin_image(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) >= 4 && lua_gettop(L) <= 5));

    struct context *w = (struct context *)lua_touserdata(L, 1);
    nk_flags flag = 0;
    struct nk_image *img;
    struct nk_vec2 size;
    const char *label = luaL_checkstring(L, 3);
    lua_check_ret(lua_nk_parse_image(w, L, 2, &img));
    lua_check_ret(lua_nk_parse_vec2(L, 4, &size));

    if (lua_gettop(L) == 5) {
        lua_check_ret(lua_nk_parse_align_text(L, 5, &flag));
    }

    bool ret;
    if (flag > 0)
        ret = nk_menu_begin_image_label(_context, label, flag, *img, size);
    else
        ret = nk_menu_begin_image(_context, label, *img, size);

    lua_pushboolean(L, ret);

    return 1;
}

int NuklearLegacyBindings::lua_nk_menu_begin_label(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) == 4));

    nk_flags flag = 0;
    struct nk_vec2 size;
    const char *label = luaL_checkstring(L, 2);
    lua_check_ret(lua_nk_parse_align_text(L, 3, &flag));
    lua_check_ret(lua_nk_parse_vec2(L, 4, &size));

    bool ret = nk_menu_begin_label(_context, label, flag, size);

    lua_pushboolean(L, ret);

    return 1;
}

int NuklearLegacyBindings::lua_nk_menu_begin_symbol(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) >= 4 && lua_gettop(L) <= 5));

    nk_symbol_type symbol = nk_symbol_type::NK_SYMBOL_NONE;
    nk_flags flag = 0;
    struct nk_vec2 size;
    const char *label = luaL_checkstring(L, 3);
    lua_check_ret(lua_nk_parse_symbol(L, 2, &symbol))
        lua_check_ret(lua_nk_parse_vec2(L, 4, &size));

    if (lua_gettop(L) == 5) {
        lua_check_ret(lua_nk_parse_align_text(L, 5, &flag));
    }

    bool ret;
    if (flag > 0)
        ret = nk_menu_begin_symbol_label(_context, label, flag, symbol, size);
    else
        ret = nk_menu_begin_symbol(_context, label, symbol, size);

    lua_pushboolean(L, ret);

    return 1;
}

int NuklearLegacyBindings::lua_nk_menu_end(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) == 1));

    nk_menu_end(_context);

    return 0;
}

int NuklearLegacyBindings::lua_nk_menu_item_image(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) == 5));

    struct context *w = (struct context *)lua_touserdata(L, 1);
    nk_flags flag = 0;
    struct nk_image *img;
    struct nk_vec2 size;
    const char *label = luaL_checkstring(L, 3);
    lua_check_ret(lua_nk_parse_image(w, L, 2, &img));
    lua_check_ret(lua_nk_parse_vec2(L, 4, &size));
    lua_check_ret(lua_nk_parse_align_text(L, 5, &flag));

    bool ret = nk_menu_item_image_label(_context, *img, label, flag);

    lua_pushboolean(L, ret);

    return 1;
}

int NuklearLegacyBindings::lua_nk_menu_item_label(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) == 3));

    nk_flags flag = 0;
    const char *label = luaL_checkstring(L, 2);
    lua_check_ret(lua_nk_parse_align_text(L, 3, &flag));

    bool ret = nk_menu_item_label(_context, label, flag);

    lua_pushboolean(L, ret);

    return 1;
}

int NuklearLegacyBindings::lua_nk_menu_item_symbol(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) == 4));

    nk_symbol_type symbol = nk_symbol_type::NK_SYMBOL_NONE;
    nk_flags flag = 0;
    const char *label = luaL_checkstring(L, 3);
    lua_check_ret(lua_nk_parse_symbol(L, 2, &symbol))
        lua_check_ret(lua_nk_parse_align_text(L, 4, &flag));

    bool ret = nk_menu_item_symbol_label(_context, symbol, label, flag);

    lua_pushboolean(L, ret);

    return 1;
}

int NuklearLegacyBindings::lua_nk_menubar_begin(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) == 1));

    nk_menubar_begin(_context);

    return 0;
}

int NuklearLegacyBindings::lua_nk_menubar_end(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) == 1));

    nk_menubar_end(_context);

    return 0;
}

int NuklearLegacyBindings::lua_nk_option_label(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) == 3));

    const char *label = luaL_checkstring(L, 2);
    nk_bool active = lua_toboolean(L, 3);

    bool ret = nk_option_label(_context, label, active);

    lua_pushboolean(L, ret);

    return 1;
}


int NuklearLegacyBindings::lua_nk_popup_begin(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) == 5));

    nk_popup_type type;
    nk_flags flags = 0;
    struct nk_rect rect;
    const char *title = luaL_checkstring(L, 3);
    lua_check_ret(lua_nk_parse_popup_type(L, 2, &type));
    lua_check_ret(lua_nk_parse_window_flags(L, 4, &flags));
    lua_check_ret(lua_nk_parse_rect(L, 5, &rect));

    bool ret = nk_popup_begin(_context, type, title, flags, rect);

    lua_pushboolean(L, ret);

    return 1;
}

int NuklearLegacyBindings::lua_nk_popup_end(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) == 1));

    nk_popup_end(_context);

    return 0;
}

int NuklearLegacyBindings::lua_nk_progress(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) == 4));

    nk_size value = luaL_checkinteger(L, 2);
    nk_size max = luaL_checkinteger(L, 3);
    nk_bool modifiable;
    lua_check_ret(lua_nk_parse_modifiable(L, 4, &modifiable));

    bool ret = nk_progress(_context, &value, max, modifiable);

    lua_pushinteger(L, value);
    lua_pushboolean(L, ret);

    return 2;
}

int NuklearLegacyBindings::lua_nk_property_float(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) == 7));

    const char *title = luaL_checkstring(L, 2);
    float min = (float)luaL_checknumber(L, 3);
    float value = (float)luaL_checknumber(L, 4);
    float max = (float)luaL_checknumber(L, 5);
    float step = (float)luaL_checknumber(L, 6);
    float inc_per_pixel = (float)luaL_checknumber(L, 7);

    nk_property_float(_context, title, min, &value, max, step, inc_per_pixel);

    lua_pushnumber(L, value);

    return 1;
}

int NuklearLegacyBindings::lua_nk_property_int(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) == 7));

    const char *title = luaL_checkstring(L, 2);
    int min = luaL_checkinteger(L, 3);
    int value = luaL_checkinteger(L, 4);
    int max = luaL_checkinteger(L, 5);
    int step = luaL_checkinteger(L, 6);
    float inc_per_pixel = (float)luaL_checknumber(L, 7);

    nk_property_int(_context, title, min, &value, max, step, inc_per_pixel);

    lua_pushinteger(L, value);

    return 1;
}

int NuklearLegacyBindings::lua_nk_propertyd(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) == 7));

    const char *title = luaL_checkstring(L, 2);
    double min = luaL_checknumber(L, 3);
    double value = luaL_checknumber(L, 4);
    double max = luaL_checknumber(L, 5);
    double step = luaL_checknumber(L, 6);
    float inc_per_pixel = (float)luaL_checknumber(L, 7);

    double ret = nk_propertyd(_context, title, min, value, max, step, inc_per_pixel);

    lua_pushnumber(L, ret);

    return 1;
}

int NuklearLegacyBindings::lua_nk_propertyf(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) == 7));

    const char *title = luaL_checkstring(L, 2);
    float min = (float)luaL_checknumber(L, 3);
    float value = (float)luaL_checknumber(L, 4);
    float max = (float)luaL_checknumber(L, 5);
    float step = (float)luaL_checknumber(L, 6);
    float inc_per_pixel = (float)luaL_checknumber(L, 7);

    float ret = nk_propertyf(_context, title, min, value, max, step, inc_per_pixel);

    lua_pushnumber(L, ret);

    return 1;
}

int NuklearLegacyBindings::lua_nk_propertyi(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) == 7));

    const char *title = luaL_checkstring(L, 2);
    int min = luaL_checkinteger(L, 3);
    int value = luaL_checkinteger(L, 4);
    int max = luaL_checkinteger(L, 5);
    int step = luaL_checkinteger(L, 6);
    float inc_per_pixel = (float)luaL_checknumber(L, 7);

    int ret = nk_propertyi(_context, title, min, value, max, step, inc_per_pixel);

    lua_pushinteger(L, ret);

    return 1;
}
int NuklearLegacyBindings::lua_nk_selectable_image(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) == 6));

    struct context *w = (struct context *)lua_touserdata(L, 1);
    nk_flags flag = 0;
    struct nk_image *img;
    struct nk_vec2 size;
    nk_bool value = lua_toboolean(L, 6);
    const char *label = luaL_checkstring(L, 3);
    lua_check_ret(lua_nk_parse_image(w, L, 2, &img));
    lua_check_ret(lua_nk_parse_vec2(L, 4, &size));
    lua_check_ret(lua_nk_parse_align_text(L, 5, &flag));

    bool ret = nk_selectable_image_label(_context, *img, label, flag, &value);

    lua_pushboolean(L, value);
    lua_pushboolean(L, ret);

    return 2;
}

int NuklearLegacyBindings::lua_nk_selectable_label(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) == 4));

    nk_flags flag = 0;
    nk_bool value = lua_toboolean(L, 4);
    const char *label = luaL_checkstring(L, 2);
    lua_check_ret(lua_nk_parse_align_text(L, 3, &flag));

    bool ret = nk_selectable_label(_context, label, flag, &value);

    lua_pushboolean(L, value);
    lua_pushboolean(L, ret);

    return 2;
}

int NuklearLegacyBindings::lua_nk_selectable_symbol(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) == 5));

    nk_symbol_type symbol = nk_symbol_type::NK_SYMBOL_NONE;
    nk_flags flag = 0;
    nk_bool value = lua_toboolean(L, 5);
    const char *label = luaL_checkstring(L, 3);
    lua_check_ret(lua_nk_parse_symbol(L, 2, &symbol))
        lua_check_ret(lua_nk_parse_align_text(L, 4, &flag));

    bool ret = nk_selectable_symbol_label(_context, symbol, label, flag, &value);

    lua_pushboolean(L, value);
    lua_pushboolean(L, ret);

    return 2;
}

int NuklearLegacyBindings::lua_nk_slide_float(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) == 5));

    float min = (float)luaL_checknumber(L, 2);
    float value = (float)luaL_checknumber(L, 3);
    float max = (float)luaL_checknumber(L, 4);
    float step = (float)luaL_checknumber(L, 5);

    float ret = nk_slide_float(_context, min, value, max, step);

    lua_pushnumber(L, ret);

    return 1;
}

int NuklearLegacyBindings::lua_nk_slide_int(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) == 5));

    int min = luaL_checkinteger(L, 2);
    int value = luaL_checkinteger(L, 3);
    int max = luaL_checkinteger(L, 4);
    int step = luaL_checkinteger(L, 5);

    int ret = nk_slide_int(_context, min, value, max, step);

    lua_pushinteger(L, ret);

    return 1;
}

int NuklearLegacyBindings::lua_nk_slider_float(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) == 5));

    float min = (float)luaL_checknumber(L, 2);
    float value = (float)luaL_checknumber(L, 3);
    float max = (float)luaL_checknumber(L, 4);
    float step = (float)luaL_checknumber(L, 5);

    bool ret = nk_slider_float(_context, min, &value, max, step);

    lua_pushnumber(L, value);
    lua_pushboolean(L, ret);

    return 2;
}

int NuklearLegacyBindings::lua_nk_slider_int(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) == 5));

    int min = luaL_checkinteger(L, 2);
    int value = luaL_checkinteger(L, 3);
    int max = luaL_checkinteger(L, 4);
    int step = luaL_checkinteger(L, 5);

    bool ret = nk_slider_int(_context, min, &value, max, step);

    lua_pushinteger(L, value);
    lua_pushboolean(L, ret);

    return 2;
}

int NuklearLegacyBindings::lua_nk_style_from_table(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) == 2));

    struct nk_color table[NK_COLOR_COUNT];
    struct nk_color color = { 0, 0, 0, 255 };

    if (lua_istable(L, 2)) {
        lua_foreach(L, 2) {
            bool is_hex_color = true;

            const char *key = luaL_checkstring(L, -2);
            lua_check_ret(lua_nk_parse_color(L, -1, &color));

            if (!strcmp(key, "border"))
                table[NK_COLOR_BORDER] = color;
            else if (!strcmp(key, "button"))
                table[NK_COLOR_BUTTON] = color;
            else if (!strcmp(key, "button_active"))
                table[NK_COLOR_BUTTON_ACTIVE] = color;
            else if (!strcmp(key, "button_hover"))
                table[NK_COLOR_BUTTON_HOVER] = color;
            else if (!strcmp(key, "chart"))
                table[NK_COLOR_CHART] = color;
            else if (!strcmp(key, "chart_color"))
                table[NK_COLOR_CHART_COLOR] = color;
            else if (!strcmp(key, "chart_color_highlight"))
                table[NK_COLOR_CHART_COLOR_HIGHLIGHT] = color;
            else if (!strcmp(key, "combo"))
                table[NK_COLOR_COMBO] = color;
            else if (!strcmp(key, "edit"))
                table[NK_COLOR_EDIT] = color;
            else if (!strcmp(key, "edit_cursor"))
                table[NK_COLOR_EDIT_CURSOR] = color;
            else if (!strcmp(key, "header"))
                table[NK_COLOR_HEADER] = color;
            else if (!strcmp(key, "property"))
                table[NK_COLOR_PROPERTY] = color;
            else if (!strcmp(key, "scrollbar"))
                table[NK_COLOR_SCROLLBAR] = color;
            else if (!strcmp(key, "scrollbar_cursor"))
                table[NK_COLOR_SCROLLBAR_CURSOR] = color;
            else if (!strcmp(key, "scrollbar_cursor_active"))
                table[NK_COLOR_SCROLLBAR_CURSOR_ACTIVE] = color;
            else if (!strcmp(key, "scrollbar_cursor_hover"))
                table[NK_COLOR_SCROLLBAR_CURSOR_HOVER] = color;
            else if (!strcmp(key, "select"))
                table[NK_COLOR_SELECT] = color;
            else if (!strcmp(key, "select_active"))
                table[NK_COLOR_SELECT_ACTIVE] = color;
            else if (!strcmp(key, "slider"))
                table[NK_COLOR_SLIDER] = color;
            else if (!strcmp(key, "slider_cursor"))
                table[NK_COLOR_SLIDER_CURSOR] = color;
            else if (!strcmp(key, "slider_cursor_active"))
                table[NK_COLOR_SLIDER_CURSOR_ACTIVE] = color;
            else if (!strcmp(key, "slider_cursor_hover"))
                table[NK_COLOR_SLIDER_CURSOR_HOVER] = color;
            else if (!strcmp(key, "tab_header"))
                table[NK_COLOR_TAB_HEADER] = color;
            else if (!strcmp(key, "text"))
                table[NK_COLOR_TEXT] = color;
            else if (!strcmp(key, "toggle"))
                table[NK_COLOR_TOGGLE] = color;
            else if (!strcmp(key, "toggle_cursor"))
                table[NK_COLOR_TOGGLE_CURSOR] = color;
            else if (!strcmp(key, "toggle_hover"))
                table[NK_COLOR_TOGGLE_HOVER] = color;
            else if (!strcmp(key, "window"))
                table[NK_COLOR_WINDOW] = color;
        }
    }
    lua_pop(L, 1);

    nk_style_from_table(_context, table);

    return 0;
}

int NuklearLegacyBindings::lua_nk_style_get(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) == 3));

    lua_nk_style_type *type;
    void *ptr = nullptr;
    nk_color *color;
    float *value;
    struct nk_vec2 *vec2;
    int i = 0;

    lua_check_ret(lua_nk_parse_style(L, 2, 3, &type, &ptr));

    bool ret = false;
    switch (*type) {
    case(lua_nk_style_type_color):
        color = (nk_color *)ptr;

        lua_nk_push_color(L, *color);
        return 1;
    case(lua_nk_style_type_float):
        value = (float *)ptr;

        lua_pushnumber(L, *value);
        return 1;
    case(lua_nk_style_type_item):
        // TODO: style item
        break;
    case(lua_nk_style_type_vec2):
        vec2 = (struct nk_vec2 *)ptr;

        lua_newtable(L);
        lua_pushliteral(L, "x");
        lua_pushnumber(L, vec2->x);
        lua_rawset(L, -3);
        lua_pushliteral(L, "y");
        lua_pushnumber(L, vec2->y);
        lua_rawset(L, -3);
        return 1;

    default:
        break;
    }

    return luaL_argerror(L, 3, lua_pushfstring(L, "not implemented yet"));
}

int NuklearLegacyBindings::lua_nk_style_pop(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) == 3));

    lua_nk_style_type *type;
    void *ptr = nullptr;
    lua_check_ret(lua_nk_parse_style(L, 2, 3, &type, &ptr));

    bool ret;
    switch (*type) {
    case(lua_nk_style_type_align_header):
    case(lua_nk_style_type_align_text):
        ret = nk_style_pop_flags(_context);
        break;
    case(lua_nk_style_type_color):
        ret = nk_style_pop_color(_context);
        break;
    case(lua_nk_style_type_float):
        ret = nk_style_pop_float(_context);
        break;
    case(lua_nk_style_type_cursor):
    case(lua_nk_style_type_item):
    case(lua_nk_style_type_symbol):
        ret = nk_style_pop_style_item(_context);
        break;
    case(lua_nk_style_type_vec2):
        ret = nk_style_pop_vec2(_context);
        break;
    default:
        return luaL_argerror(L, 3, lua_pushfstring(L, "not implemented style property"));
    }

    lua_pushboolean(L, ret);

    return 1;
}

int NuklearLegacyBindings::lua_nk_style_push(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) == 4));

    struct context *w = (struct context *)lua_touserdata(L, 1);
    lua_nk_style_type *type;
    void *ptr = nullptr;
    nk_color color;
    float value;
    struct nk_image *img;
    struct nk_vec2 vec2;
    nk_flags flags;

    lua_check_ret(lua_nk_parse_style(L, 2, 3, &type, &ptr));

    bool ret;
    switch (*type) {
    case(lua_nk_style_type_align_header):
        lua_check_ret(lua_nk_parse_align_header(L, 4, &flags));
        ret = nk_style_push_flags(_context, (nk_flags *)ptr, flags);
        break;
    case(lua_nk_style_type_align_text):
        lua_check_ret(lua_nk_parse_align_text(L, 4, &flags));
        ret = nk_style_push_flags(_context, (nk_flags *)ptr, flags);
        break;
    case(lua_nk_style_type_color):
        lua_check_ret(lua_nk_parse_color(L, 4, &color));
        ret = nk_style_push_color(_context, (nk_color *)ptr, color);
        break;
    case(lua_nk_style_type_float):
        value = (float)luaL_checknumber(L, 4);
        ret = nk_style_push_float(_context, (float *)ptr, value);
        break;
    case(lua_nk_style_type_item):
        if (lua_isnumber(L, 4)) {
            lua_check_ret(lua_nk_parse_image(w, L, 4, &img));
            ret = nk_style_push_style_item(_context, (nk_style_item *)ptr, nk_style_item_image(*img));
        } else if (lua_istable(L, 4) || lua_isstring(L, 4)) {
            lua_check_ret(lua_nk_parse_color(L, 4, &color));
            ret = nk_style_push_style_item(_context, (nk_style_item *)ptr, nk_style_item_color(color));
        } else {
            // TODO: nine slice
            return luaL_argerror(L, 4, lua_pushfstring(L, "not implemented yet"));
        }

        break;
    case(lua_nk_style_type_vec2):
        lua_check_ret(lua_nk_parse_vec2(L, 4, &vec2));
        ret = nk_style_push_vec2(_context, (struct nk_vec2 *)ptr, vec2);
        break;
    default:
        return luaL_argerror(L, 3, lua_pushfstring(L, "not implemented style property"));
    }

    lua_pushboolean(L, ret);

    return 1;
}

int NuklearLegacyBindings::lua_nk_style_set(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) == 4));

    struct context *w = (struct context *)lua_touserdata(L, 1);
    lua_nk_style_type *type;
    void *ptr = nullptr;
    nk_color color;
    float value;
    struct nk_image *img;
    nk_style_item item;

    lua_check_ret(lua_nk_parse_style(L, 2, 3, &type, &ptr));

    switch (*type) {
    case(lua_nk_style_type_align_header):
        lua_check_ret(lua_nk_parse_align_header(L, 4, (nk_flags *)ptr));
        break;
    case(lua_nk_style_type_align_text):
        lua_check_ret(lua_nk_parse_align_text(L, 4, (nk_flags *)ptr));
        break;
    case(lua_nk_style_type_color):
        lua_check_ret(lua_nk_parse_color(L, 4, (nk_color *)ptr));
        break;
    case(lua_nk_style_type_float):
        value = (float)luaL_checknumber(L, 4);
        memcpy(ptr, &value, sizeof(float));
        break;
    case(lua_nk_style_type_item):
        if (lua_isnumber(L, 4)) {
            lua_check_ret(lua_nk_parse_image(w, L, 4, &img));
            item = nk_style_item_image(*img);
            memcpy(ptr, &item, sizeof(nk_style_item));
        } else if (lua_istable(L, 4) || lua_isstring(L, 4)) {
            lua_check_ret(lua_nk_parse_color(L, 4, &color));
            item = nk_style_item_color(color);
            memcpy(ptr, &item, sizeof(nk_style_item));
        } else {
            // TODO: nine slice
            return luaL_argerror(L, 4, lua_pushfstring(L, "not implemented yet"));
        }

        break;
    case(lua_nk_style_type_vec2):
        lua_check_ret(lua_nk_parse_vec2(L, 4, (struct nk_vec2 *)ptr));
        break;
    default:
        return luaL_argerror(L, 3, lua_pushfstring(L, "not implemented style property"));
    }

    return 0;
}

int NuklearLegacyBindings::lua_nk_style_set_font(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) == 2));

    struct context *w = (struct context *)lua_touserdata(L, 1);
    struct nk_tex_font *font = (struct nk_tex_font *)lua_touserdata(L, 2);

    if (font)
        nk_style_set_font(_context, &font->font->handle);

    return 0;
}

int NuklearLegacyBindings::lua_nk_style_set_font_default(lua_State *L) {
    // Need to be reimplemented if we really need it
    assert(0);

    lua_check_ret(lua_check_args(L, lua_gettop(L) == 1));

    struct context *w = (struct context *)lua_touserdata(L, 1);

    //nk_style_set_font(_context, &font_default.font->handle);

    return 0;
}

int NuklearLegacyBindings::lua_nk_spacer(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) == 1));

    nk_spacer(_context);

    return 0;
}

int NuklearLegacyBindings::lua_nk_tree_element_pop(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) == 1));

    nk_tree_element_pop(_context);

    return 0;
}

int NuklearLegacyBindings::lua_nk_tree_element_push(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) == 4));

    nk_tree_type type;
    nk_collapse_states states;
    nk_bool value;
    const char *label = luaL_checkstring(L, 3);
    lua_check_ret(lua_nk_parse_tree_type(L, 2, &type));
    lua_check_ret(lua_nk_parse_collapse_states(L, 4, &states));

    // TODO: replace __LINE__ with wins context
    bool ret = nk_tree_element_push_hashed(_context, type, label, states, &value, label, strlen(label), __LINE__);

    lua_pushboolean(L, value);
    lua_pushboolean(L, ret);

    return 2;
}

int NuklearLegacyBindings::lua_nk_tree_pop(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) == 1));

    nk_tree_pop(_context);

    return 0;
}

int NuklearLegacyBindings::lua_nk_tree_push(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) == 4));

    nk_tree_type type;
    nk_collapse_states states;
    const char *label = luaL_checkstring(L, 3);
    lua_check_ret(lua_nk_parse_tree_type(L, 2, &type));
    lua_check_ret(lua_nk_parse_collapse_states(L, 4, &states));

    // TODO: replace __LINE__ with wins context
    bool ret = nk_tree_push_hashed(_context, type, label, states, label, strlen(label), __LINE__);

    lua_pushboolean(L, ret);

    return 1;
}

int NuklearLegacyBindings::lua_nk_tree_state_pop(lua_State *L) {
    assert(lua_gettop(L) == 0);

    nk_tree_state_pop(_context);

    return 0;
}

int NuklearLegacyBindings::lua_nk_tree_state_push(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) == 4));

    nk_tree_type type;
    nk_collapse_states states;
    const char *label = luaL_checkstring(L, 3);
    lua_check_ret(lua_nk_parse_tree_type(L, 2, &type));
    lua_check_ret(lua_nk_parse_collapse_states(L, 4, &states));

    bool ret = nk_tree_state_push(_context, type, label, &states);

    lua_pushboolean(L, states == NK_MAXIMIZED ? true : false);
    lua_pushboolean(L, ret);

    return 2;
}

int NuklearLegacyBindings::lua_nk_window_is_closed(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) == 2));

    const char *title = luaL_checkstring(L, 2);

    bool ret = nk_window_is_closed(_context, title);

    lua_pushboolean(L, ret);

    return 1;
}

int NuklearLegacyBindings::lua_nk_window_is_hidden(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) == 2));

    const char *title = luaL_checkstring(L, 2);

    bool ret = nk_window_is_hidden(_context, title);

    lua_pushboolean(L, ret);

    return 1;
}

int NuklearLegacyBindings::lua_nk_window_is_hovered(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) == 1));
    bool ret = nk_window_is_hovered(_context);

    lua_pushboolean(L, ret);

    return 1;
}

int NuklearLegacyBindings::lua_nk_window_get_size(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) == 1));

    struct nk_vec2 size = nk_window_get_size(_context);

    lua_pushnumber(L, size.x);
    lua_pushnumber(L, size.y);

    return 2;
}

int NuklearLegacyBindings::lua_nk_window_set_size(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) == 3));

    const char *name = lua_tostring(L, 2);
    struct nk_vec2 size;
    lua_check_ret(lua_nk_parse_vec2(L, 3, &size));

    nk_window_set_size(_context, name, size);

    return 0;
}

int NuklearLegacyBindings::lua_nk_window_set_position(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) == 3));

    const char *name = lua_tostring(L, 2);
    struct nk_vec2 size;
    lua_check_ret(lua_nk_parse_vec2(L, 3, &size));

    nk_window_set_size(_context, name, size);

    return 0;
}

int NuklearLegacyBindings::lua_nk_window_set_bounds(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) == 3));

    const char *name = lua_tostring(L, 2);
    struct nk_rect bounds;
    lua_check_ret(lua_nk_parse_rect(L, 3, &bounds));

    nk_window_set_bounds(_context, name, bounds);

    return 0;
}

int NuklearLegacyBindings::lua_nk_window_get_position(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) == 1));

    struct nk_vec2 size = nk_window_get_position(_context);

    lua_pushnumber(L, size.x);
    lua_pushnumber(L, size.y);

    return 2;
}

int NuklearLegacyBindings::lua_nk_load_image(lua_State *L) {
    lua_check_ret(lua_check_args(L, lua_gettop(L) >= 3 && lua_gettop(L) <= 5));

    struct context *w = (struct context *)lua_touserdata(L, 1);
    const char *type = luaL_checkstring(L, 2);
    const char *name = luaL_checkstring(L, 3);
    const char *lod = nullptr;
    Color mask = colorTable.TealMask;
    if (lua_gettop(L) == 4)
        mask = Color::fromC16(luaL_checkinteger(L, 4));
    if (lua_gettop(L) == 5)
        lod = luaL_checkstring(L, 5);

    struct img *im = new(struct img);

    int slot = -1;
    bool ret = false;
    if (lod) {
        // TODO: load from custom lod when this functionality becomes available
        // LodReader lodReader(lod);
    } else {
        if (!strcmp(type, "pcx"))
            im->asset = assets->getImage_PCXFromIconsLOD(name);
        else if (!strcmp(type, "bmp"))
            im->asset = assets->getImage_ColorKey(name, mask);
    }

    if (!im->asset) {
        delete im;
        lua_pushnil(L);
        return 1;
    }

    auto texid = im->asset->renderId().value();
    im->nk = nk_image_id(texid);

    slot = w->imgs.size();
    w->imgs.push_back(im);

    //logger->info("Nuklear: [{}] asset {}: '{}', type '{}' loaded!", w->tmpl, slot, name, type);

    lua_pushnumber(L, slot);

    return 1;
}

int NuklearLegacyBindings::lua_nk_scroll_new(lua_State *L) {
    lua_check_ret(lua_check_args_count(L, lua_gettop(L) >= 1 && lua_gettop(L) <= 2));

    int x = 0, y = 0;
    if (lua_gettop(L) >= 1) {
        x = luaL_checkint(L, 1);
    }
    if (lua_gettop(L) >= 2) {
        y = luaL_checkint(L, 2);
    }
    nk_scroll *scroll = (nk_scroll *)lua_newuserdata(L, sizeof(nk_scroll));
    scroll->x = x;
    scroll->y = y;
    luaL_getmetatable(L, "nk_scroll_mt");
    lua_setmetatable(L, -2);
    return 1;
}

int NuklearLegacyBindings::lua_nk_scroll_set(lua_State *L) {
    lua_check_ret(lua_check_args_count(L, lua_gettop(L) >= 3));

    nk_scroll *scroll = lua_nk_check_scroll(L, 1);
    scroll->x = luaL_checkint(L, 2);
    scroll->y = luaL_checkint(L, 3);
    return 0;
}

void NuklearLegacyBindings::initBindings(lua_State* lua) {
    initStyles();
    initNkScrollType(lua);
    static const luaL_Reg ui[] = {
        { "window_begin", lua_nk_begin },
        { "button_color", lua_nk_button_color },
        { "button_image", lua_nk_button_image },
        { "button_label", lua_nk_button_label },
        { "button_set_behavior", lua_nk_button_set_behavior },
        { "button_symbol", lua_nk_button_symbol },
        { "chart_begin", lua_nk_chart_begin },
        { "chart_end", lua_nk_chart_end },
        { "chart_push", lua_nk_chart_push },
        { "checkbox", lua_nk_checkbox },
        { "checkbox_label", lua_nk_checkbox },
        { "color_picker", lua_nk_color_picker },
        { "color_update", lua_nk_color_update },
        { "combo", lua_nk_combo },
        { "combo_begin_color", lua_nk_combo_begin_color },
        { "combo_begin_image", lua_nk_combo_begin_image },
        { "combo_begin_label", lua_nk_combo_begin_label },
        { "combo_begin_symbol", lua_nk_combo_begin_symbol },
        { "combo_close", lua_nk_combo_close },
        { "combo_item_image", lua_nk_combo_item_image },
        { "combo_item_label", lua_nk_combo_item_label },
        { "combo_item_symbol", lua_nk_combo_item_symbol },
        { "combo_end", lua_nk_combo_end },
        { "edit_string", lua_nk_edit_string },
        { "window_end", lua_nk_end },
        { "group_begin", lua_nk_group_begin },
        { "group_end", lua_nk_group_end },
        { "group_scrolled_begin", lua_nk_group_scrolled_begin },
        { "group_scrolled_end", lua_nk_group_scrolled_end },
        { "group_set_scroll", lua_nk_group_set_scroll },
        { "image_dimensions", lua_nk_image_dimensions },
        { "label", lua_nk_label },
        { "label_colored", lua_nk_label_colored },
        { "load_image", lua_nk_load_image },
        { "layout_row_begin", lua_nk_layout_row_begin },
        { "layout_row_dynamic", lua_nk_layout_row_dynamic },
        { "layout_row_push", lua_nk_layout_row_push },
        { "layout_row_static", lua_nk_layout_row_static },
        { "layout_row_end", lua_nk_layout_row_end },
        { "layout_space_begin", lua_nk_layout_space_begin},
        { "layout_space_end", lua_nk_layout_space_end },
        { "layout_space_push", lua_nk_layout_space_push },
        { "layout_reset_min_row_height", lua_nk_layout_reset_min_row_height },
        { "menu_begin_image", lua_nk_menu_begin_image },
        { "menu_begin_label", lua_nk_menu_begin_label },
        { "menu_begin_symbol", lua_nk_menu_begin_symbol },
        { "menu_end", lua_nk_menu_end },
        { "menu_item_image", lua_nk_menu_item_image },
        { "menu_item_label", lua_nk_menu_item_label },
        { "menu_item_symbol", lua_nk_menu_item_symbol },
        { "menubar_begin", lua_nk_menubar_begin },
        { "menubar_end", lua_nk_menubar_end },
        { "option_label", lua_nk_option_label },
        { "popup_begin", lua_nk_popup_begin },
        { "popup_end", lua_nk_popup_end },
        { "progress", lua_nk_progress },
        { "property_float", lua_nk_property_float },
        { "property_int", lua_nk_property_int },
        { "propertyd", lua_nk_propertyd },
        { "propertyf", lua_nk_propertyf },
        { "propertyi", lua_nk_propertyi },
        { "selectable_image", lua_nk_selectable_image },
        { "selectable_label", lua_nk_selectable_label },
        { "selectable_symbol", lua_nk_selectable_symbol },
        { "slide_float", lua_nk_slide_float },
        { "slide_int", lua_nk_slide_int },
        { "slider_float", lua_nk_slider_float },
        { "slider_int", lua_nk_slider_int },
        { "spacer", lua_nk_spacer },
        { "style_default", lua_nk_style_default },
        { "style_from_table", lua_nk_style_from_table },
        { "style_get", lua_nk_style_get },
        { "style_pop", lua_nk_style_pop },
        { "style_push", lua_nk_style_push },
        { "style_set", lua_nk_style_set },
        { "style_set_font", lua_nk_style_set_font },
        { "style_set_font_default", lua_nk_style_set_font_default },
        { "tree_element_pop", lua_nk_tree_element_pop },
        { "tree_element_push", lua_nk_tree_element_push },
        { "tree_pop", lua_nk_tree_pop },
        { "tree_push", lua_nk_tree_push },
        { "tree_state_pop", lua_nk_tree_state_pop },
        { "tree_state_push", lua_nk_tree_state_push },
        { "window_is_closed", lua_nk_window_is_closed },
        { "window_is_hidden", lua_nk_window_is_hidden },
        { "window_is_hovered", lua_nk_window_is_hovered },
        { "window_get_size", lua_nk_window_get_size },
        { "window_set_size", lua_nk_window_set_size },
        { "window_get_position", lua_nk_window_get_position },
        { "window_set_position", lua_nk_window_set_position },
        { "window_set_bounds", lua_nk_window_set_bounds },
        { NULL, NULL }
    };
    luaL_newlib(lua, ui);
    lua_setglobal(lua, "_nuklear_legacy_ui");
}

void NuklearLegacyBindings::initNkScrollType(lua_State *lua) {
    static const luaL_Reg nk_scroll[] = {
        { "new", lua_nk_scroll_new },
        { "set", lua_nk_scroll_set },
        { NULL, NULL }
    };
    luaL_newlib(lua, nk_scroll);
    lua_setglobal(lua, "nk_scroll");

    luaL_newmetatable(lua, "nk_scroll_mt");
    lua_pushstring(lua, "__index");
    lua_pushvalue(lua, -2);
    lua_settable(lua, -3);
    luaL_openlib(lua, nullptr, nk_scroll, 0);
    lua_pop(lua, -1);
}

#define PUSH_STYLE(element, component, property, type) element.push_back({ #property, &_context->style.component.property, type});

void NuklearLegacyBindings::initStyles() {
    struct lua_nk_style style;
    style.props.clear();

    style.component = "button";
    /* button background */
    PUSH_STYLE(style.props, button, active, lua_nk_style_type_item);
    PUSH_STYLE(style.props, button, border_color, lua_nk_style_type_color);
    PUSH_STYLE(style.props, button, hover, lua_nk_style_type_item);
    PUSH_STYLE(style.props, button, normal, lua_nk_style_type_item);
    /* button text */
    PUSH_STYLE(style.props, button, text_active, lua_nk_style_type_color);
    PUSH_STYLE(style.props, button, text_alignment, lua_nk_style_type_align_text);
    PUSH_STYLE(style.props, button, text_background, lua_nk_style_type_color);
    PUSH_STYLE(style.props, button, text_hover, lua_nk_style_type_color);
    PUSH_STYLE(style.props, button, text_normal, lua_nk_style_type_color);
    /* button properties */
    PUSH_STYLE(style.props, button, border, lua_nk_style_type_float);
    PUSH_STYLE(style.props, button, image_padding, lua_nk_style_type_vec2);
    PUSH_STYLE(style.props, button, padding, lua_nk_style_type_vec2);
    PUSH_STYLE(style.props, button, rounding, lua_nk_style_type_float);
    PUSH_STYLE(style.props, button, touch_padding, lua_nk_style_type_vec2);
    _styles.push_back(style);
    style.props.clear();

    style.component = "chart";
    /* chart background */
    PUSH_STYLE(style.props, chart, background, lua_nk_style_type_item);
    PUSH_STYLE(style.props, chart, border_color, lua_nk_style_type_color);
    PUSH_STYLE(style.props, chart, color, lua_nk_style_type_color);
    PUSH_STYLE(style.props, chart, selected_color, lua_nk_style_type_color);
    /* chart properties */
    PUSH_STYLE(style.props, chart, border, lua_nk_style_type_float);
    PUSH_STYLE(style.props, chart, padding, lua_nk_style_type_vec2);
    PUSH_STYLE(style.props, chart, rounding, lua_nk_style_type_float);
    _styles.push_back(style);
    style.props.clear();

    style.component = "checkbox";
    /* background */
    PUSH_STYLE(style.props, checkbox, active, lua_nk_style_type_item);
    PUSH_STYLE(style.props, checkbox, border_color, lua_nk_style_type_color);
    PUSH_STYLE(style.props, checkbox, hover, lua_nk_style_type_item);
    PUSH_STYLE(style.props, checkbox, normal, lua_nk_style_type_item);
    /* cursor */
    PUSH_STYLE(style.props, checkbox, cursor_hover, lua_nk_style_type_cursor);
    PUSH_STYLE(style.props, checkbox, cursor_normal, lua_nk_style_type_cursor);
    /* text */
    PUSH_STYLE(style.props, checkbox, text_active, lua_nk_style_type_color);
    PUSH_STYLE(style.props, checkbox, text_alignment, lua_nk_style_type_align_text);
    PUSH_STYLE(style.props, checkbox, text_background, lua_nk_style_type_color);
    PUSH_STYLE(style.props, checkbox, text_normal, lua_nk_style_type_color);
    PUSH_STYLE(style.props, checkbox, text_hover, lua_nk_style_type_color);
    /* properties */
    PUSH_STYLE(style.props, checkbox, border, lua_nk_style_type_float);
    PUSH_STYLE(style.props, checkbox, padding, lua_nk_style_type_float);
    PUSH_STYLE(style.props, checkbox, spacing, lua_nk_style_type_float);
    PUSH_STYLE(style.props, checkbox, touch_padding, lua_nk_style_type_vec2);
    _styles.push_back(style);
    style.props.clear();

    style.component = "combo";
    /* combo background */
    PUSH_STYLE(style.props, combo, active, lua_nk_style_type_item);
    PUSH_STYLE(style.props, combo, border_color, lua_nk_style_type_color);
    PUSH_STYLE(style.props, combo, hover, lua_nk_style_type_item);
    PUSH_STYLE(style.props, combo, normal, lua_nk_style_type_item);
    /* combo text */
    PUSH_STYLE(style.props, combo, label_active, lua_nk_style_type_color);
    PUSH_STYLE(style.props, combo, label_normal, lua_nk_style_type_color);
    PUSH_STYLE(style.props, combo, label_hover, lua_nk_style_type_color);
    /* combo symbol */
    PUSH_STYLE(style.props, combo, symbol_active, lua_nk_style_type_symbol);
    PUSH_STYLE(style.props, combo, symbol_hover, lua_nk_style_type_symbol);
    PUSH_STYLE(style.props, combo, symbol_normal, lua_nk_style_type_symbol);
    /* combo button */
    {
        /* button background */
        PUSH_STYLE(style.props, combo, button.active, lua_nk_style_type_item);
        PUSH_STYLE(style.props, combo, button.border_color, lua_nk_style_type_color);
        PUSH_STYLE(style.props, combo, button.hover, lua_nk_style_type_item);
        PUSH_STYLE(style.props, combo, button.normal, lua_nk_style_type_item);
        /* button text */
        PUSH_STYLE(style.props, combo, button.text_active, lua_nk_style_type_color);
        PUSH_STYLE(style.props, combo, button.text_alignment, lua_nk_style_type_align_text);
        PUSH_STYLE(style.props, combo, button.text_background, lua_nk_style_type_color);
        PUSH_STYLE(style.props, combo, button.text_hover, lua_nk_style_type_color);
        PUSH_STYLE(style.props, combo, button.text_normal, lua_nk_style_type_color);
        /* button properties */
        PUSH_STYLE(style.props, combo, button.border, lua_nk_style_type_float);
        PUSH_STYLE(style.props, combo, button.image_padding, lua_nk_style_type_vec2);
        PUSH_STYLE(style.props, combo, button.padding, lua_nk_style_type_vec2);
        PUSH_STYLE(style.props, combo, button.rounding, lua_nk_style_type_float);
        PUSH_STYLE(style.props, combo, button.touch_padding, lua_nk_style_type_vec2);
    }
    PUSH_STYLE(style.props, combo, sym_active, lua_nk_style_type_symbol);
    PUSH_STYLE(style.props, combo, sym_hover, lua_nk_style_type_symbol);
    PUSH_STYLE(style.props, combo, sym_normal, lua_nk_style_type_symbol);
    /* combo properties */
    PUSH_STYLE(style.props, combo, border, lua_nk_style_type_float);
    PUSH_STYLE(style.props, combo, button_padding, lua_nk_style_type_vec2);
    PUSH_STYLE(style.props, combo, content_padding, lua_nk_style_type_vec2);
    PUSH_STYLE(style.props, combo, rounding, lua_nk_style_type_float);
    PUSH_STYLE(style.props, combo, spacing, lua_nk_style_type_vec2);
    _styles.push_back(style);
    style.props.clear();

    style.component = "edit";
    /* edit background */
    PUSH_STYLE(style.props, edit, active, lua_nk_style_type_item);
    PUSH_STYLE(style.props, edit, border_color, lua_nk_style_type_color);
    PUSH_STYLE(style.props, edit, hover, lua_nk_style_type_item);
    PUSH_STYLE(style.props, edit, normal, lua_nk_style_type_item);
    /* edit scrollbar */
    {
        PUSH_STYLE(style.props, edit, scrollbar.active, lua_nk_style_type_item);
        PUSH_STYLE(style.props, edit, scrollbar.border_color, lua_nk_style_type_color);
        PUSH_STYLE(style.props, edit, scrollbar.hover, lua_nk_style_type_item);
        PUSH_STYLE(style.props, edit, scrollbar.normal, lua_nk_style_type_item);

        /* cursor */
        PUSH_STYLE(style.props, edit, scrollbar.cursor_active, lua_nk_style_type_cursor);
        PUSH_STYLE(style.props, edit, scrollbar.cursor_border_color, lua_nk_style_type_color);
        PUSH_STYLE(style.props, edit, scrollbar.cursor_hover, lua_nk_style_type_cursor);
        PUSH_STYLE(style.props, edit, scrollbar.cursor_normal, lua_nk_style_type_cursor);

        /* properties */
        PUSH_STYLE(style.props, edit, scrollbar.border_cursor, lua_nk_style_type_float);
        PUSH_STYLE(style.props, edit, scrollbar.rounding_cursor, lua_nk_style_type_float);
        PUSH_STYLE(style.props, edit, scrollbar.border, lua_nk_style_type_float);
        PUSH_STYLE(style.props, edit, scrollbar.padding, lua_nk_style_type_vec2);
        PUSH_STYLE(style.props, edit, scrollbar.rounding, lua_nk_style_type_float);

        /* optional buttons */
        PUSH_STYLE(style.props, edit.scrollbar, show_buttons, lua_nk_style_type_integer);
        {
            /* button background */
            PUSH_STYLE(style.props, edit, scrollbar.dec_button.active, lua_nk_style_type_item);
            PUSH_STYLE(style.props, edit, scrollbar.dec_button.border_color, lua_nk_style_type_color);
            PUSH_STYLE(style.props, edit, scrollbar.dec_button.hover, lua_nk_style_type_item);
            PUSH_STYLE(style.props, edit, scrollbar.dec_button.normal, lua_nk_style_type_item);
            /* button text */
            PUSH_STYLE(style.props, edit, scrollbar.dec_button.text_active, lua_nk_style_type_color);
            PUSH_STYLE(style.props, edit, scrollbar.dec_button.text_alignment, lua_nk_style_type_align_text);
            PUSH_STYLE(style.props, edit, scrollbar.dec_button.text_background, lua_nk_style_type_color);
            PUSH_STYLE(style.props, edit, scrollbar.dec_button.text_hover, lua_nk_style_type_color);
            PUSH_STYLE(style.props, edit, scrollbar.dec_button.text_normal, lua_nk_style_type_color);
            /* button properties */
            PUSH_STYLE(style.props, edit, scrollbar.dec_button.border, lua_nk_style_type_float);
            PUSH_STYLE(style.props, edit, scrollbar.dec_button.image_padding, lua_nk_style_type_vec2);
            PUSH_STYLE(style.props, edit, scrollbar.dec_button.padding, lua_nk_style_type_vec2);
            PUSH_STYLE(style.props, edit, scrollbar.dec_button.rounding, lua_nk_style_type_float);
            PUSH_STYLE(style.props, edit, scrollbar.dec_button.touch_padding, lua_nk_style_type_vec2);
        }
        PUSH_STYLE(style.props, edit, scrollbar.dec_symbol, lua_nk_style_type_symbol);
        {
            /* button background */
            PUSH_STYLE(style.props, edit, scrollbar.inc_button.active, lua_nk_style_type_item);
            PUSH_STYLE(style.props, edit, scrollbar.inc_button.border_color, lua_nk_style_type_color);
            PUSH_STYLE(style.props, edit, scrollbar.inc_button.hover, lua_nk_style_type_item);
            PUSH_STYLE(style.props, edit, scrollbar.inc_button.normal, lua_nk_style_type_item);
            /* button text */
            PUSH_STYLE(style.props, edit, scrollbar.inc_button.text_active, lua_nk_style_type_color);
            PUSH_STYLE(style.props, edit, scrollbar.inc_button.text_alignment, lua_nk_style_type_align_text);
            PUSH_STYLE(style.props, edit, scrollbar.inc_button.text_background, lua_nk_style_type_color);
            PUSH_STYLE(style.props, edit, scrollbar.inc_button.text_hover, lua_nk_style_type_color);
            PUSH_STYLE(style.props, edit, scrollbar.inc_button.text_normal, lua_nk_style_type_color);
            /* button properties */
            PUSH_STYLE(style.props, edit, scrollbar.inc_button.border, lua_nk_style_type_float);
            PUSH_STYLE(style.props, edit, scrollbar.inc_button.image_padding, lua_nk_style_type_vec2);
            PUSH_STYLE(style.props, edit, scrollbar.inc_button.padding, lua_nk_style_type_vec2);
            PUSH_STYLE(style.props, edit, scrollbar.inc_button.rounding, lua_nk_style_type_float);
            PUSH_STYLE(style.props, edit, scrollbar.inc_button.touch_padding, lua_nk_style_type_vec2);
        }
        PUSH_STYLE(style.props, edit, scrollbar.inc_symbol, lua_nk_style_type_symbol);
    }
    /* edit cursor */
    PUSH_STYLE(style.props, edit, cursor_hover, lua_nk_style_type_cursor);
    PUSH_STYLE(style.props, edit, cursor_normal, lua_nk_style_type_cursor);
    PUSH_STYLE(style.props, edit, cursor_text_hover, lua_nk_style_type_cursor);
    PUSH_STYLE(style.props, edit, cursor_text_normal, lua_nk_style_type_cursor);
    /* edit text (unselected) */
    PUSH_STYLE(style.props, edit, text_active, lua_nk_style_type_color);
    PUSH_STYLE(style.props, edit, text_hover, lua_nk_style_type_color);
    PUSH_STYLE(style.props, edit, text_normal, lua_nk_style_type_color);
    /* edit text (selected) */
    PUSH_STYLE(style.props, edit, selected_hover, lua_nk_style_type_color);
    PUSH_STYLE(style.props, edit, selected_normal, lua_nk_style_type_color);
    PUSH_STYLE(style.props, edit, selected_text_hover, lua_nk_style_type_color);
    PUSH_STYLE(style.props, edit, selected_text_normal, lua_nk_style_type_color);
    /* edit properties */
    PUSH_STYLE(style.props, edit, cursor_size, lua_nk_style_type_float);
    PUSH_STYLE(style.props, edit, border, lua_nk_style_type_float);
    PUSH_STYLE(style.props, edit, padding, lua_nk_style_type_vec2);
    PUSH_STYLE(style.props, edit, rounding, lua_nk_style_type_float);
    PUSH_STYLE(style.props, edit, row_padding, lua_nk_style_type_float);
    PUSH_STYLE(style.props, edit, scrollbar_size, lua_nk_style_type_vec2);
    _styles.push_back(style);
    style.props.clear();

    style.component = "progress";
    /* background */
    PUSH_STYLE(style.props, progress, active, lua_nk_style_type_item);
    PUSH_STYLE(style.props, progress, border_color, lua_nk_style_type_color);
    PUSH_STYLE(style.props, progress, hover, lua_nk_style_type_item);
    PUSH_STYLE(style.props, progress, normal, lua_nk_style_type_item);

    /* cursor */
    PUSH_STYLE(style.props, progress, cursor_active, lua_nk_style_type_cursor);
    PUSH_STYLE(style.props, progress, cursor_border_color, lua_nk_style_type_color);
    PUSH_STYLE(style.props, progress, cursor_hover, lua_nk_style_type_cursor);
    PUSH_STYLE(style.props, progress, cursor_normal, lua_nk_style_type_cursor);

    /* properties */
    PUSH_STYLE(style.props, progress, cursor_border, lua_nk_style_type_float);
    PUSH_STYLE(style.props, progress, cursor_rounding, lua_nk_style_type_float);
    PUSH_STYLE(style.props, progress, border, lua_nk_style_type_float);
    PUSH_STYLE(style.props, progress, padding, lua_nk_style_type_vec2);
    PUSH_STYLE(style.props, progress, rounding, lua_nk_style_type_float);
    _styles.push_back(style);
    style.props.clear();

    style.component = "property";
    /* background */
    PUSH_STYLE(style.props, property, active, lua_nk_style_type_item);
    PUSH_STYLE(style.props, property, border_color, lua_nk_style_type_color);
    PUSH_STYLE(style.props, property, hover, lua_nk_style_type_item);
    PUSH_STYLE(style.props, property, normal, lua_nk_style_type_item);

    /* text */
    PUSH_STYLE(style.props, property, label_active, lua_nk_style_type_color);
    PUSH_STYLE(style.props, property, label_normal, lua_nk_style_type_color);
    PUSH_STYLE(style.props, property, label_hover, lua_nk_style_type_color);

    /* symbols */
    PUSH_STYLE(style.props, property, sym_left, lua_nk_style_type_symbol);
    PUSH_STYLE(style.props, property, sym_right, lua_nk_style_type_symbol);

    /* properties */
    PUSH_STYLE(style.props, property, border, lua_nk_style_type_float);
    /* dec_button */
    {
        /* button background */
        PUSH_STYLE(style.props, property.dec_button, active, lua_nk_style_type_item);
        PUSH_STYLE(style.props, property.dec_button, border_color, lua_nk_style_type_color);
        PUSH_STYLE(style.props, property.dec_button, hover, lua_nk_style_type_item);
        PUSH_STYLE(style.props, property.dec_button, normal, lua_nk_style_type_item);
        /* button text */
        PUSH_STYLE(style.props, property.dec_button, text_active, lua_nk_style_type_color);
        PUSH_STYLE(style.props, property.dec_button, text_alignment, lua_nk_style_type_align_text);
        PUSH_STYLE(style.props, property.dec_button, text_background, lua_nk_style_type_color);
        PUSH_STYLE(style.props, property.dec_button, text_hover, lua_nk_style_type_color);
        PUSH_STYLE(style.props, property.dec_button, text_normal, lua_nk_style_type_color);
        /* button properties */
        PUSH_STYLE(style.props, property.dec_button, border, lua_nk_style_type_float);
        PUSH_STYLE(style.props, property.dec_button, image_padding, lua_nk_style_type_vec2);
        PUSH_STYLE(style.props, property.dec_button, padding, lua_nk_style_type_vec2);
        PUSH_STYLE(style.props, property.dec_button, rounding, lua_nk_style_type_float);
        PUSH_STYLE(style.props, property.dec_button, touch_padding, lua_nk_style_type_vec2);
    }
    PUSH_STYLE(style.props, property, edit, lua_nk_style_type_edit);
    /* inc_button */
    {
        /* button background */
        PUSH_STYLE(style.props, property.inc_button, active, lua_nk_style_type_item);
        PUSH_STYLE(style.props, property.inc_button, border_color, lua_nk_style_type_color);
        PUSH_STYLE(style.props, property.inc_button, hover, lua_nk_style_type_item);
        PUSH_STYLE(style.props, property.inc_button, normal, lua_nk_style_type_item);
        /* button text */
        PUSH_STYLE(style.props, property.inc_button, text_active, lua_nk_style_type_color);
        PUSH_STYLE(style.props, property.inc_button, text_alignment, lua_nk_style_type_align_text);
        PUSH_STYLE(style.props, property.inc_button, text_background, lua_nk_style_type_color);
        PUSH_STYLE(style.props, property.inc_button, text_hover, lua_nk_style_type_color);
        PUSH_STYLE(style.props, property.inc_button, text_normal, lua_nk_style_type_color);
        /* button properties */
        PUSH_STYLE(style.props, property.inc_button, border, lua_nk_style_type_float);
        PUSH_STYLE(style.props, property.inc_button, image_padding, lua_nk_style_type_vec2);
        PUSH_STYLE(style.props, property.inc_button, padding, lua_nk_style_type_vec2);
        PUSH_STYLE(style.props, property.inc_button, rounding, lua_nk_style_type_float);
        PUSH_STYLE(style.props, property.inc_button, touch_padding, lua_nk_style_type_vec2);
    }
    PUSH_STYLE(style.props, property, padding, lua_nk_style_type_vec2);
    PUSH_STYLE(style.props, property, rounding, lua_nk_style_type_float);
    _styles.push_back(style);
    style.props.clear();

    style.component = "selectable";
    /* background (inactive) */
    PUSH_STYLE(style.props, selectable, hover, lua_nk_style_type_item);
    PUSH_STYLE(style.props, selectable, normal, lua_nk_style_type_item);
    PUSH_STYLE(style.props, selectable, pressed, lua_nk_style_type_item);

    /* background (active) */
    PUSH_STYLE(style.props, selectable, hover_active, lua_nk_style_type_item);
    PUSH_STYLE(style.props, selectable, normal_active, lua_nk_style_type_item);
    PUSH_STYLE(style.props, selectable, pressed_active, lua_nk_style_type_item);

    /* text (inactive) */
    PUSH_STYLE(style.props, selectable, text_normal, lua_nk_style_type_color);
    PUSH_STYLE(style.props, selectable, text_hover, lua_nk_style_type_color);
    PUSH_STYLE(style.props, selectable, text_pressed, lua_nk_style_type_color);

    /* text (active) */
    PUSH_STYLE(style.props, selectable, text_alignment, lua_nk_style_type_align_text);
    PUSH_STYLE(style.props, selectable, text_background, lua_nk_style_type_color);
    PUSH_STYLE(style.props, selectable, text_normal_active, lua_nk_style_type_color);
    PUSH_STYLE(style.props, selectable, text_hover_active, lua_nk_style_type_color);
    PUSH_STYLE(style.props, selectable, text_pressed_active, lua_nk_style_type_color);

    /* properties */
    PUSH_STYLE(style.props, selectable, padding, lua_nk_style_type_vec2);
    PUSH_STYLE(style.props, selectable, rounding, lua_nk_style_type_float);
    PUSH_STYLE(style.props, selectable, image_padding, lua_nk_style_type_vec2);
    PUSH_STYLE(style.props, selectable, touch_padding, lua_nk_style_type_vec2);
    _styles.push_back(style);
    style.props.clear();

    style.component = "slider";
    PUSH_STYLE(style.props, slider, active, lua_nk_style_type_item);
    PUSH_STYLE(style.props, slider, border_color, lua_nk_style_type_color);
    PUSH_STYLE(style.props, slider, hover, lua_nk_style_type_item);
    PUSH_STYLE(style.props, slider, normal, lua_nk_style_type_item);

    /* background bar */
    PUSH_STYLE(style.props, slider, bar_active, lua_nk_style_type_color);
    PUSH_STYLE(style.props, slider, bar_filled, lua_nk_style_type_color);
    PUSH_STYLE(style.props, slider, bar_hover, lua_nk_style_type_color);
    PUSH_STYLE(style.props, slider, bar_normal, lua_nk_style_type_color);

    /* cursor */
    PUSH_STYLE(style.props, slider, cursor_active, lua_nk_style_type_cursor);
    PUSH_STYLE(style.props, slider, cursor_hover, lua_nk_style_type_cursor);
    PUSH_STYLE(style.props, slider, cursor_normal, lua_nk_style_type_cursor);

    /* symbol */
    PUSH_STYLE(style.props, slider, dec_symbol, lua_nk_style_type_symbol);
    PUSH_STYLE(style.props, slider, inc_symbol, lua_nk_style_type_symbol);

    /* dec_button */
    {
        /* button background */
        PUSH_STYLE(style.props, slider, dec_button.active, lua_nk_style_type_item);
        PUSH_STYLE(style.props, slider, dec_button.border_color, lua_nk_style_type_color);
        PUSH_STYLE(style.props, slider, dec_button.hover, lua_nk_style_type_item);
        PUSH_STYLE(style.props, slider, dec_button.normal, lua_nk_style_type_item);
        /* button text */
        PUSH_STYLE(style.props, slider, dec_button.text_active, lua_nk_style_type_color);
        PUSH_STYLE(style.props, slider, dec_button.text_alignment, lua_nk_style_type_align_text);
        PUSH_STYLE(style.props, slider, dec_button.text_background, lua_nk_style_type_color);
        PUSH_STYLE(style.props, slider, dec_button.text_hover, lua_nk_style_type_color);
        PUSH_STYLE(style.props, slider, dec_button.text_normal, lua_nk_style_type_color);
        /* button properties */
        PUSH_STYLE(style.props, slider, dec_button.border, lua_nk_style_type_float);
        PUSH_STYLE(style.props, slider, dec_button.image_padding, lua_nk_style_type_vec2);
        PUSH_STYLE(style.props, slider, dec_button.padding, lua_nk_style_type_vec2);
        PUSH_STYLE(style.props, slider, dec_button.rounding, lua_nk_style_type_float);
        PUSH_STYLE(style.props, slider, dec_button.touch_padding, lua_nk_style_type_vec2);
    }
    /* inc_button */
    {
        /* button background */
        PUSH_STYLE(style.props, slider, inc_button.active, lua_nk_style_type_item);
        PUSH_STYLE(style.props, slider, inc_button.border_color, lua_nk_style_type_color);
        PUSH_STYLE(style.props, slider, inc_button.hover, lua_nk_style_type_item);
        PUSH_STYLE(style.props, slider, inc_button.normal, lua_nk_style_type_item);
        /* button text */
        PUSH_STYLE(style.props, slider, inc_button.text_active, lua_nk_style_type_color);
        PUSH_STYLE(style.props, slider, inc_button.text_alignment, lua_nk_style_type_align_text);
        PUSH_STYLE(style.props, slider, inc_button.text_background, lua_nk_style_type_color);
        PUSH_STYLE(style.props, slider, inc_button.text_hover, lua_nk_style_type_color);
        PUSH_STYLE(style.props, slider, inc_button.text_normal, lua_nk_style_type_color);
        /* button properties */
        PUSH_STYLE(style.props, slider, inc_button.border, lua_nk_style_type_float);
        PUSH_STYLE(style.props, slider, inc_button.image_padding, lua_nk_style_type_vec2);
        PUSH_STYLE(style.props, slider, inc_button.padding, lua_nk_style_type_vec2);
        PUSH_STYLE(style.props, slider, inc_button.rounding, lua_nk_style_type_float);
        PUSH_STYLE(style.props, slider, inc_button.touch_padding, lua_nk_style_type_vec2);
    }

    /* properties */
    PUSH_STYLE(style.props, slider, bar_height, lua_nk_style_type_float);
    PUSH_STYLE(style.props, slider, border, lua_nk_style_type_float);
    PUSH_STYLE(style.props, slider, cursor_size, lua_nk_style_type_vec2);
    PUSH_STYLE(style.props, slider, padding, lua_nk_style_type_vec2);
    PUSH_STYLE(style.props, slider, spacing, lua_nk_style_type_vec2);
    PUSH_STYLE(style.props, slider, show_buttons, lua_nk_style_type_integer);
    _styles.push_back(style);
    style.props.clear();

    style.component = "tab";
    /* background */
    PUSH_STYLE(style.props, tab, background, lua_nk_style_type_item);
    PUSH_STYLE(style.props, tab, border_color, lua_nk_style_type_color);
    PUSH_STYLE(style.props, tab, text, lua_nk_style_type_color);
    /* node_maximize_button */
    {
        /* button background */
        PUSH_STYLE(style.props, tab, node_maximize_button.active, lua_nk_style_type_item);
        PUSH_STYLE(style.props, tab, node_maximize_button.border_color, lua_nk_style_type_color);
        PUSH_STYLE(style.props, tab, node_maximize_button.hover, lua_nk_style_type_item);
        PUSH_STYLE(style.props, tab, node_maximize_button.normal, lua_nk_style_type_item);
        /* button text */
        PUSH_STYLE(style.props, tab, node_maximize_button.text_active, lua_nk_style_type_color);
        PUSH_STYLE(style.props, tab, node_maximize_button.text_alignment, lua_nk_style_type_align_text);
        PUSH_STYLE(style.props, tab, node_maximize_button.text_background, lua_nk_style_type_color);
        PUSH_STYLE(style.props, tab, node_maximize_button.text_hover, lua_nk_style_type_color);
        PUSH_STYLE(style.props, tab, node_maximize_button.text_normal, lua_nk_style_type_color);
        /* button properties */
        PUSH_STYLE(style.props, tab, node_maximize_button.border, lua_nk_style_type_float);
        PUSH_STYLE(style.props, tab, node_maximize_button.image_padding, lua_nk_style_type_vec2);
        PUSH_STYLE(style.props, tab, node_maximize_button.padding, lua_nk_style_type_vec2);
        PUSH_STYLE(style.props, tab, node_maximize_button.rounding, lua_nk_style_type_float);
        PUSH_STYLE(style.props, tab, node_maximize_button.touch_padding, lua_nk_style_type_vec2);
    }
    /* node_minimize_button */
    {
        /* button background */
        PUSH_STYLE(style.props, tab, node_minimize_button.active, lua_nk_style_type_item);
        PUSH_STYLE(style.props, tab, node_minimize_button.border_color, lua_nk_style_type_color);
        PUSH_STYLE(style.props, tab, node_minimize_button.hover, lua_nk_style_type_item);
        PUSH_STYLE(style.props, tab, node_minimize_button.normal, lua_nk_style_type_item);
        /* button text */
        PUSH_STYLE(style.props, tab, node_minimize_button.text_active, lua_nk_style_type_color);
        PUSH_STYLE(style.props, tab, node_minimize_button.text_alignment, lua_nk_style_type_align_text);
        PUSH_STYLE(style.props, tab, node_minimize_button.text_background, lua_nk_style_type_color);
        PUSH_STYLE(style.props, tab, node_minimize_button.text_hover, lua_nk_style_type_color);
        PUSH_STYLE(style.props, tab, node_minimize_button.text_normal, lua_nk_style_type_color);
        /* button properties */
        PUSH_STYLE(style.props, tab, node_minimize_button.border, lua_nk_style_type_float);
        PUSH_STYLE(style.props, tab, node_minimize_button.image_padding, lua_nk_style_type_vec2);
        PUSH_STYLE(style.props, tab, node_minimize_button.padding, lua_nk_style_type_vec2);
        PUSH_STYLE(style.props, tab, node_minimize_button.rounding, lua_nk_style_type_float);
        PUSH_STYLE(style.props, tab, node_minimize_button.touch_padding, lua_nk_style_type_vec2);
    }
    /* tab_maximize_button */
    {
        /* button background */
        PUSH_STYLE(style.props, tab, tab_maximize_button.active, lua_nk_style_type_item);
        PUSH_STYLE(style.props, tab, tab_maximize_button.border_color, lua_nk_style_type_color);
        PUSH_STYLE(style.props, tab, tab_maximize_button.hover, lua_nk_style_type_item);
        PUSH_STYLE(style.props, tab, tab_maximize_button.normal, lua_nk_style_type_item);
        /* button text */
        PUSH_STYLE(style.props, tab, tab_maximize_button.text_active, lua_nk_style_type_color);
        PUSH_STYLE(style.props, tab, tab_maximize_button.text_alignment, lua_nk_style_type_align_text);
        PUSH_STYLE(style.props, tab, tab_maximize_button.text_background, lua_nk_style_type_color);
        PUSH_STYLE(style.props, tab, tab_maximize_button.text_hover, lua_nk_style_type_color);
        PUSH_STYLE(style.props, tab, tab_maximize_button.text_normal, lua_nk_style_type_color);
        /* button properties */
        PUSH_STYLE(style.props, tab, tab_maximize_button.border, lua_nk_style_type_float);
        PUSH_STYLE(style.props, tab, tab_maximize_button.image_padding, lua_nk_style_type_vec2);
        PUSH_STYLE(style.props, tab, tab_maximize_button.padding, lua_nk_style_type_vec2);
        PUSH_STYLE(style.props, tab, tab_maximize_button.rounding, lua_nk_style_type_float);
        PUSH_STYLE(style.props, tab, tab_maximize_button.touch_padding, lua_nk_style_type_vec2);
    }
    /* tab_minimize_button */
    {
        /* button background */
        PUSH_STYLE(style.props, tab, tab_minimize_button.active, lua_nk_style_type_item);
        PUSH_STYLE(style.props, tab, tab_minimize_button.border_color, lua_nk_style_type_color);
        PUSH_STYLE(style.props, tab, tab_minimize_button.hover, lua_nk_style_type_item);
        PUSH_STYLE(style.props, tab, tab_minimize_button.normal, lua_nk_style_type_item);
        /* button text */
        PUSH_STYLE(style.props, tab, tab_minimize_button.text_active, lua_nk_style_type_color);
        PUSH_STYLE(style.props, tab, tab_minimize_button.text_alignment, lua_nk_style_type_align_text);
        PUSH_STYLE(style.props, tab, tab_minimize_button.text_background, lua_nk_style_type_color);
        PUSH_STYLE(style.props, tab, tab_minimize_button.text_hover, lua_nk_style_type_color);
        PUSH_STYLE(style.props, tab, tab_minimize_button.text_normal, lua_nk_style_type_color);
        /* button properties */
        PUSH_STYLE(style.props, tab, tab_minimize_button.border, lua_nk_style_type_float);
        PUSH_STYLE(style.props, tab, tab_minimize_button.image_padding, lua_nk_style_type_vec2);
        PUSH_STYLE(style.props, tab, tab_minimize_button.padding, lua_nk_style_type_vec2);
        PUSH_STYLE(style.props, tab, tab_minimize_button.rounding, lua_nk_style_type_float);
        PUSH_STYLE(style.props, tab, tab_minimize_button.touch_padding, lua_nk_style_type_vec2);
    }
    /* symbol */
    PUSH_STYLE(style.props, tab, sym_maximize, lua_nk_style_type_symbol);
    PUSH_STYLE(style.props, tab, sym_minimize, lua_nk_style_type_symbol);

    /* properties */
    PUSH_STYLE(style.props, tab, border, lua_nk_style_type_float);
    PUSH_STYLE(style.props, tab, indent, lua_nk_style_type_float);
    PUSH_STYLE(style.props, tab, padding, lua_nk_style_type_vec2);
    PUSH_STYLE(style.props, tab, spacing, lua_nk_style_type_vec2);
    PUSH_STYLE(style.props, tab, rounding, lua_nk_style_type_float);
    _styles.push_back(style);
    style.props.clear();

    style.component = "window.header";
    /* background */
    PUSH_STYLE(style.props, window.header, active, lua_nk_style_type_item);
    PUSH_STYLE(style.props, window.header, normal, lua_nk_style_type_item);
    PUSH_STYLE(style.props, window.header, hover, lua_nk_style_type_item);
    /* close_button */
    {
        /* button background */
        PUSH_STYLE(style.props, window.header, close_button.active, lua_nk_style_type_item);
        PUSH_STYLE(style.props, window.header, close_button.border_color, lua_nk_style_type_color);
        PUSH_STYLE(style.props, window.header, close_button.hover, lua_nk_style_type_item);
        PUSH_STYLE(style.props, window.header, close_button.normal, lua_nk_style_type_item);
        /* button text */
        PUSH_STYLE(style.props, window.header, close_button.text_active, lua_nk_style_type_color);
        PUSH_STYLE(style.props, window.header, close_button.text_alignment, lua_nk_style_type_align_text);
        PUSH_STYLE(style.props, window.header, close_button.text_background, lua_nk_style_type_color);
        PUSH_STYLE(style.props, window.header, close_button.text_hover, lua_nk_style_type_color);
        PUSH_STYLE(style.props, window.header, close_button.text_normal, lua_nk_style_type_color);
        /* button properties */
        PUSH_STYLE(style.props, window.header, close_button.border, lua_nk_style_type_float);
        PUSH_STYLE(style.props, window.header, close_button.image_padding, lua_nk_style_type_vec2);
        PUSH_STYLE(style.props, window.header, close_button.padding, lua_nk_style_type_vec2);
        PUSH_STYLE(style.props, window.header, close_button.rounding, lua_nk_style_type_float);
        PUSH_STYLE(style.props, window.header, close_button.touch_padding, lua_nk_style_type_vec2);
    }
    /* minimize_button */
    {
        /* button background */
        PUSH_STYLE(style.props, window.header, minimize_button.active, lua_nk_style_type_item);
        PUSH_STYLE(style.props, window.header, minimize_button.border_color, lua_nk_style_type_color);
        PUSH_STYLE(style.props, window.header, minimize_button.hover, lua_nk_style_type_item);
        PUSH_STYLE(style.props, window.header, minimize_button.normal, lua_nk_style_type_item);
        /* button text */
        PUSH_STYLE(style.props, window.header, minimize_button.text_active, lua_nk_style_type_color);
        PUSH_STYLE(style.props, window.header, minimize_button.text_alignment, lua_nk_style_type_align_text);
        PUSH_STYLE(style.props, window.header, minimize_button.text_background, lua_nk_style_type_color);
        PUSH_STYLE(style.props, window.header, minimize_button.text_hover, lua_nk_style_type_color);
        PUSH_STYLE(style.props, window.header, minimize_button.text_normal, lua_nk_style_type_color);
        /* button properties */
        PUSH_STYLE(style.props, window.header, minimize_button.border, lua_nk_style_type_float);
        PUSH_STYLE(style.props, window.header, minimize_button.image_padding, lua_nk_style_type_vec2);
        PUSH_STYLE(style.props, window.header, minimize_button.padding, lua_nk_style_type_vec2);
        PUSH_STYLE(style.props, window.header, minimize_button.rounding, lua_nk_style_type_float);
        PUSH_STYLE(style.props, window.header, minimize_button.touch_padding, lua_nk_style_type_vec2);
    }
    /* symbol */
    PUSH_STYLE(style.props, window.header, close_symbol, lua_nk_style_type_symbol);
    PUSH_STYLE(style.props, window.header, minimize_symbol, lua_nk_style_type_symbol);
    PUSH_STYLE(style.props, window.header, maximize_symbol, lua_nk_style_type_symbol);

    /* title */
    PUSH_STYLE(style.props, window.header, label_active, lua_nk_style_type_color);
    PUSH_STYLE(style.props, window.header, label_hover, lua_nk_style_type_color);
    PUSH_STYLE(style.props, window.header, label_normal, lua_nk_style_type_color);

    /* properties */
    PUSH_STYLE(style.props, window.header, align, lua_nk_style_type_align_header);
    PUSH_STYLE(style.props, window.header, label_padding, lua_nk_style_type_vec2);
    PUSH_STYLE(style.props, window.header, padding, lua_nk_style_type_vec2);
    PUSH_STYLE(style.props, window.header, spacing, lua_nk_style_type_vec2);
    _styles.push_back(style);
    style.props.clear();

    style.component = "window";
    /* window background */
    PUSH_STYLE(style.props, window, fixed_background, lua_nk_style_type_item);
    PUSH_STYLE(style.props, window, background, lua_nk_style_type_color);
    /* window colors */
    PUSH_STYLE(style.props, window, border_color, lua_nk_style_type_color);
    PUSH_STYLE(style.props, window, combo_border_color, lua_nk_style_type_color);
    PUSH_STYLE(style.props, window, contextual_border_color, lua_nk_style_type_color);
    PUSH_STYLE(style.props, window, group_border_color, lua_nk_style_type_color);
    PUSH_STYLE(style.props, window, menu_border_color, lua_nk_style_type_color);
    PUSH_STYLE(style.props, window, popup_border_color, lua_nk_style_type_color);
    PUSH_STYLE(style.props, window, tooltip_border_color, lua_nk_style_type_color);
    /* window border */
    PUSH_STYLE(style.props, window, border, lua_nk_style_type_float);
    PUSH_STYLE(style.props, window, combo_border, lua_nk_style_type_float);
    PUSH_STYLE(style.props, window, contextual_border, lua_nk_style_type_float);
    PUSH_STYLE(style.props, window, group_border, lua_nk_style_type_float);
    PUSH_STYLE(style.props, window, menu_border, lua_nk_style_type_float);
    PUSH_STYLE(style.props, window, popup_border, lua_nk_style_type_float);
    PUSH_STYLE(style.props, window, tooltip_border, lua_nk_style_type_float);
    /* window padding */
    PUSH_STYLE(style.props, window, combo_padding, lua_nk_style_type_vec2);
    PUSH_STYLE(style.props, window, contextual_padding, lua_nk_style_type_vec2);
    PUSH_STYLE(style.props, window, group_padding, lua_nk_style_type_vec2);
    PUSH_STYLE(style.props, window, menu_padding, lua_nk_style_type_vec2);
    PUSH_STYLE(style.props, window, min_row_height_padding, lua_nk_style_type_float);
    PUSH_STYLE(style.props, window, popup_padding, lua_nk_style_type_vec2);
    PUSH_STYLE(style.props, window, tooltip_padding, lua_nk_style_type_vec2);
    /* window properties */
    PUSH_STYLE(style.props, window, min_size, lua_nk_style_type_vec2);
    PUSH_STYLE(style.props, window, padding, lua_nk_style_type_vec2);
    PUSH_STYLE(style.props, window, scrollbar_size, lua_nk_style_type_vec2);
    PUSH_STYLE(style.props, window, spacing, lua_nk_style_type_vec2);
    _styles.push_back(style);
}
