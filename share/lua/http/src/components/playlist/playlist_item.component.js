Vue.component('playlist-item', {
    template: '#playlist-item-template',
    props: ['item'],
    computed: {
        ...Vuex.mapState({
            playlist: state => state.playlist.items
        }),
    },
    methods: {
        addItem(mode, id, title, src) {
            this.$store.dispatch('playlist/addItem', src);
        },
        removeItem(id) {
            this.$store.dispatch('playlist/removeItem', id);
        },
        play(src, id) {
            this.$store.dispatch('status/play', id);
        }
    },
    created() {

    }
});
