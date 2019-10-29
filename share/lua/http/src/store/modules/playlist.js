import playlistService from '../../services/playlist.service.js';

// initial state
const state = {
    items: []
};

const getters = {};

const actions = {
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
    setPlaylist(state, playlist) {
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
