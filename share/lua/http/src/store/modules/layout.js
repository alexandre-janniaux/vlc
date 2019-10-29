// initial state
const state = {
    playlist: true,
    navbar: true
};

const getters = {};

const actions = {
    openPlaylist({ commit }) {
        commit('setPlaylistOpened', true);
    },
    closePlaylist({ commit }) {
        commit('setPlaylistOpened', false);
    },
    openNavbar({ commit }) {
        commit('setNavbarOpened', true);
    },
    closeNavbar({ commit }) {
        commit('setNavbarOpened', false);
    }
};

const mutations = {
    setPlaylistOpened(state, opened) {
        state.playlist = opened;
    },
    setNavbarOpened(state, opened) {
        state.navbar = opened;
    },
};

export default {
    namespaced: true,
    state,
    getters,
    actions,
    mutations
};
