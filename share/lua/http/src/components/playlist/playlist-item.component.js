Vue.component('playlist-item', {
    template: '#playlist-item-template',
    props: ['item'],
    computed: {
        ...Vuex.mapState({
            playlist: state => state.playlist
        }),
    },
    methods: {
        addItem(mode, id, title, src) {
            this.$store.dispatch('playlist/addItem', src);
        },
        play(src, id) {
            this.$store.dispatch('layout/setPlayerView');
            this.$store.dispatch('status/play', id);
        },
        onImgError(item) {
            item.src = 'http://images.videolan.org/images/VLC-IconSmall.png';
        },
        setActiveItem(item) {
            this.$store.dispatch('playlist/setActiveItem', item);
        }
    },
    created() {

    }
});
