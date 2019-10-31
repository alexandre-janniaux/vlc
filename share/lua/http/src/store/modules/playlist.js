import playlistService from '../../services/playlist.service.js';

// initial state
const state = {
    items: [],
};

const getters = {
    getCurrentItem: (state, getters, rootState) => {
        const currentItem = state.items.filter((state) => {
            return state.current === 'current';
        });
        return currentItem.length ? currentItem[0] : null;
    }
};

const actions = {
    setActiveItem({ commit }, item) {
        // Some set actuve request
        commit('setActiveItem', item);
    },
    fetchPlaylist({ commit }) {
        playlistService.fetchPlaylist()
            .then((playlist) => {
                commit('setPlaylist', playlist);
            });
    },
    addItem({ commit, dispatch }, src) {
        playlistService.addItem(src)
            .then(() => {
                // Refresh playlist
                dispatch('fetchPlaylist');
            });
    },
    removeItem({ commit }, id) {
        playlistService.removeItem(id)
            .then(() => {
                commit('removeItem', id);
            });
    },
};

const mutations = {
    setActiveItem(state, item) {
        state.activeItem = item;
        state.items = state.items.map((s) => {
            s.active = item && item.id === s.id;
            return s;
        });
    },
    setPlaylist(state, playlist) {
        const origin = location.origin;
        playlist = playlist.map((pl) => {
            pl.src = origin + '?item=' + pl.id;
            pl.active = state.activeItem && state.activeItem.id === pl.id;
            return pl;
        });
        state.items = playlist;
    },
    removeItem(state, id) {
        return state.items = state.items.filter((item) => {
            return item.id !== id;
        });
    }
};

export default {
    namespaced: true,
    state,
    getters,
    actions,
    mutations
};
